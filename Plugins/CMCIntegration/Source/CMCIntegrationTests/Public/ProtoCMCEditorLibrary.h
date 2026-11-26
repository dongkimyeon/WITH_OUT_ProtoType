#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProtoCMCEditorLibrary.generated.h"

UCLASS()
class CMCINTEGRATIONTESTS_API UProtoCMCEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="CMC Integration")
    static void NormalizeComponentNames(UBlueprint* Blueprint, bool RestoreTemplateNames = false);

    UFUNCTION(BlueprintCallable, Category="CMC Integration")
    static TArray<FString> DescribeStruct(UObject* Object, FName PropertyName);

    UFUNCTION(BlueprintCallable, Category="CMC Integration")
    static TArray<FString> CopyGameplaySockets(class USkeletalMesh* Source, class USkeletalMesh* Target);

    UFUNCTION(BlueprintCallable, Category="CMC Integration")
    static TArray<FString> DescribeGraph(UBlueprint* Blueprint, FName GraphName);

    UFUNCTION(BlueprintCallable, Category="CMC Integration")
    static bool EnsureParentBeginPlay(UBlueprint* Blueprint);
};
