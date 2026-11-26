#include "ProtoCMCEditorLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/UnrealType.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_Event.h"
#include "K2Node_CallParentFunction.h"
#include "EdGraphSchema_K2.h"

void UProtoCMCEditorLibrary::NormalizeComponentNames(UBlueprint* Blueprint, bool RestoreTemplateNames)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript) return;
    const TArray<USCS_Node*> Nodes = Blueprint->SimpleConstructionScript->GetAllNodes();
    for (USCS_Node* Node : Nodes)
    {
        const FString Name = Node->GetVariableName().ToString();
        if (RestoreTemplateNames)
        {
            if (Name.StartsWith(TEXT("CMC_")))
                FBlueprintEditorUtils::RenameComponentMemberVariable(Blueprint, Node, FName(Name.RightChop(4)));
        }
        else if (Name.StartsWith(TEXT("Legacy_")))
            FBlueprintEditorUtils::RenameComponentMemberVariable(Blueprint, Node, FName(Name.RightChop(7)));
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

TArray<FString> UProtoCMCEditorLibrary::DescribeStruct(UObject* Object, FName PropertyName)
{
    TArray<FString> Result;
    const FStructProperty* Struct = Object ? FindFProperty<FStructProperty>(Object->GetClass(), PropertyName) : nullptr;
    if (Struct)
        for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It)
            Result.Add(It->GetName() + TEXT("|") + It->GetAuthoredName() + TEXT("|") + It->GetCPPType());
    return Result;
}

TArray<FString> UProtoCMCEditorLibrary::CopyGameplaySockets(USkeletalMesh* Source, USkeletalMesh* Target)
{
    TArray<FString> Result;
    if (!Source || !Target) return Result;
    for (FName Name : {FName(TEXT("WeaponSocket")), FName(TEXT("PistolSocket")), FName(TEXT("WeaponStorage")), FName(TEXT("PistolStorage"))})
    {
        const USkeletalMeshSocket* Socket = Source->FindSocket(Name);
        if (!Socket || Target->GetRefSkeleton().FindBoneIndex(Socket->BoneName) == INDEX_NONE)
        {
            Result.Add(TEXT("MISSING:") + Name.ToString());
            continue;
        }
        if (!Target->FindSocket(Name))
        {
            Target->Modify();
            auto Copy = DuplicateObject<USkeletalMeshSocket>(Socket, Target);
            Target->GetMeshOnlySocketList().Add(Copy);
            Target->MarkPackageDirty();
        }
        Result.Add(Name.ToString() + TEXT(":") + Socket->BoneName.ToString());
    }
    return Result;
}

TArray<FString> UProtoCMCEditorLibrary::DescribeGraph(UBlueprint* Blueprint, FName GraphName)
{
    TArray<FString> Result;
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
        if (Graph->GetFName() == GraphName)
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                FString Line = Node->GetName() + TEXT("|") + Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    Line += TEXT("|") + Pin->PinName.ToString() + TEXT("=") + Pin->DefaultValue;
                    for (UEdGraphPin* Link : Pin->LinkedTo)
                        Line += TEXT("->") + Link->GetOwningNode()->GetName() + TEXT(".") + Link->PinName.ToString();
                }
                Result.Add(Line);
            }
    return Result;
}

bool UProtoCMCEditorLibrary::EnsureParentBeginPlay(UBlueprint* Blueprint)
{
    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto Event = Cast<UK2Node_Event>(Node))
                if (Event->EventReference.GetMemberName() == TEXT("ReceiveBeginPlay"))
                {
                    for (UEdGraphNode* Other : Graph->Nodes)
                        if (auto Call = Cast<UK2Node_CallParentFunction>(Other))
                            if (Call->FunctionReference.GetMemberName() == TEXT("ReceiveBeginPlay")) return true;
                    UFunction* Function = Blueprint->ParentClass->FindFunctionByName(TEXT("ReceiveBeginPlay"));
                    if (!Function) return false;
                    FGraphNodeCreator<UK2Node_CallParentFunction> Creator(*Graph);
                    auto Call = Creator.CreateNode();
                    Call->SetFromFunction(Function);
                    Call->NodePosX = Event->NodePosX + 220;
                    Call->NodePosY = Event->NodePosY + 120;
                    Creator.Finalize();
                    UEdGraphPin* Then = Event->FindPin(UEdGraphSchema_K2::PN_Then);
                    if (!Then) return false;
                    auto Links = Then->LinkedTo;
                    Then->BreakAllPinLinks();
                    const UEdGraphSchema* Schema = Graph->GetSchema();
                    if (!Schema->TryCreateConnection(Then, Call->GetExecPin())) return false;
                    for (UEdGraphPin* Link : Links)
                        if (!Schema->TryCreateConnection(Call->GetThenPin(), Link)) return false;
                    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
                    return true;
                }
    return true;
}
