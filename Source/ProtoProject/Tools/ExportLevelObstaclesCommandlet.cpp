#include "ExportLevelObstaclesCommandlet.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "Engine/StaticMesh.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogExportLevelObstacles, Log, All);

int32 UExportLevelObstaclesCommandlet::Main(const FString& Params)
{
	FString MapPackageName;
	if (!FParse::Value(*Params, TEXT("Map="), MapPackageName) || MapPackageName.IsEmpty())
	{
		UE_LOG(LogExportLevelObstacles, Error,
			TEXT("Usage: -run=ExportLevelObstacles -Map=/Game/Maps/L_Stage2 -Out=<path.txt>"));
		return 1;
	}

	FString OutPath;
	if (!FParse::Value(*Params, TEXT("Out="), OutPath) || OutPath.IsEmpty())
	{
		OutPath = FPaths::ProjectSavedDir() / TEXT("LevelObstacles.txt");
	}

	UPackage* Package = LoadPackage(nullptr, *MapPackageName, LOAD_None);
	if (!Package)
	{
		UE_LOG(LogExportLevelObstacles, Error, TEXT("Failed to load map package '%s'"), *MapPackageName);
		return 1;
	}

	UWorld* World = UWorld::FindWorldInPackage(Package);
	if (!World || !World->PersistentLevel)
	{
		UE_LOG(LogExportLevelObstacles, Error, TEXT("Package '%s' has no world/persistent level"), *MapPackageName);
		return 1;
	}

	UE_LOG(LogExportLevelObstacles, Display, TEXT("Persistent level '%s' has %d actors; %d streaming level(s)"),
		*World->PersistentLevel->GetName(), World->PersistentLevel->Actors.Num(), World->GetStreamingLevels().Num());
	for (ULevelStreaming* StreamingLevel : World->GetStreamingLevels())
	{
		if (StreamingLevel)
		{
			UE_LOG(LogExportLevelObstacles, Display, TEXT("  streaming level: %s"), *StreamingLevel->GetWorldAssetPackageName());
		}
	}

	// 2D (X/Y) AABBs only -- WOP_SERVER's steering never looks at height,
	// it just needs "is this rectangle in the way of a straight line".
	TArray<FString> Lines;
	int32 Count = 0;
	for (AActor* Actor : World->PersistentLevel->Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}

		// GetActorBounds()/UPrimitiveComponent::Bounds depend on the render
		// scene's live bounds cache, which this bare commandlet never
		// populates (no rendering device -- everything comes back
		// zero-extent even for real geometry). Instead, compute bounds
		// directly from each UStaticMeshComponent's *asset* (UStaticMesh::
		// GetBounds(), which is just serialized data, no render state
		// needed) transformed into world space by hand.
		bool bHasBounds = false;
		FBox WorldBox(ForceInit);
		bool bBlocksPawn = false;

		for (UActorComponent* Component : Actor->GetComponents())
		{
			const UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Component);
			if (!MeshComp || !MeshComp->GetStaticMesh())
			{
				continue;
			}

			if (MeshComp->IsCollisionEnabled() &&
				MeshComp->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
			{
				bBlocksPawn = true;
			}

			const FBoxSphereBounds MeshBounds = MeshComp->GetStaticMesh()->GetBounds();
			const FBox LocalBox = FBox::BuildAABB(MeshBounds.Origin, MeshBounds.BoxExtent);
			const FBox ComponentWorldBox = LocalBox.TransformBy(MeshComp->GetComponentTransform());
			WorldBox += ComponentWorldBox;
			bHasBounds = true;
		}

		if (Count < 30 && bHasBounds)
		{
			UE_LOG(LogExportLevelObstacles, Display, TEXT("  actor '%s' (%s): blocksPawn=%d box=%s"),
				*Actor->GetName(), *Actor->GetClass()->GetName(), bBlocksPawn, *WorldBox.ToString());
		}

		if (!bBlocksPawn || !bHasBounds || WorldBox.GetExtent().IsNearlyZero())
		{
			continue;
		}

		const FVector Min = WorldBox.Min;
		const FVector Max = WorldBox.Max;
		Lines.Add(FString::Printf(TEXT("%.1f %.1f %.1f %.1f"), Min.X, Min.Y, Max.X, Max.Y));
		++Count;
	}

	if (!FFileHelper::SaveStringArrayToFile(Lines, *OutPath))
	{
		UE_LOG(LogExportLevelObstacles, Error, TEXT("Failed to write '%s'"), *OutPath);
		return 1;
	}

	UE_LOG(LogExportLevelObstacles, Display, TEXT("Exported %d blocking obstacles from '%s' to '%s'"),
		Count, *MapPackageName, *OutPath);
	return 0;
}
