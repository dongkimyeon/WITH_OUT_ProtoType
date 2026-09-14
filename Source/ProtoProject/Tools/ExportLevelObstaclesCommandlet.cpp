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
#if WITH_EDITOR
#include "Editor.h"
#include "FileHelpers.h"
#endif

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

	UWorld* World = nullptr;

#if WITH_EDITOR
	// 근본 원인 확정 (사용자가 에디터에서 직접 확인한 실제 장애물 좌표 vs
	// [DIAG] 진단 로그 대조로 확인, UpdateWorldComponents()로도 미해결):
	// LoadPackage() + FindWorldInPackage()만으로 얻은 UWorld는 "File > Open
	// Level"이 실제로 여는 그 월드와 다르다 -- 액터 컴포넌트의
	// ComponentToWorld가 항등행렬(=월드 원점)로 남는다(934개 중 933개가
	// 정확히 X=0.000으로 찍혔었음). UEditorLoadingAndSavingUtils::LoadMap은
	// 에디터가 레벨을 열 때 실제로 호출하는 바로 그 함수라 트랜스폼이
	// 제대로 계산된 진짜 에디터 월드를 돌려준다.
	if (GEditor)
	{
		UEditorLoadingAndSavingUtils::LoadMap(MapPackageName);
		World = GEditor->GetEditorWorldContext().World();
	}
#endif

	if (!World)
	{
		// 에디터 컨텍스트가 없는 경우(WITH_EDITOR=0 또는 GEditor==nullptr)를
		// 위한 폴백 -- 위 LoadMap 경로보다 신뢰도가 낮다(이 함수의 본문
		// 주석 참고).
		UPackage* Package = LoadPackage(nullptr, *MapPackageName, LOAD_None);
		if (!Package)
		{
			UE_LOG(LogExportLevelObstacles, Error, TEXT("Failed to load map package '%s'"), *MapPackageName);
			return 1;
		}
		World = UWorld::FindWorldInPackage(Package);
	}

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

	// 진단용 (일시적): 커맨드릿이 실행되는 헤드리스 컨텍스트에서 액터의
	// "순수 위치"(GetActorLocation, 컴포넌트 바운드 계산과 무관 -- RootComponent
	// 트랜스폼만 읽음)가 실제로 맞게 읽히는지부터 확인한다. 필터링 없이 전부
	// 찍어서, 플레이 구역 근처(사용자가 에디터에서 직접 좌표 확인한 실제
	// 장애물 위치) 액터가 존재하는지, 있다면 그 GetActorLocation()이 실제
	// 좌표와 맞는지를 grep으로 나중에 대조한다.
	for (AActor* DiagActor : World->PersistentLevel->Actors)
	{
		if (!IsValid(DiagActor))
		{
			continue;
		}
		UE_LOG(LogExportLevelObstacles, Display, TEXT("[DIAG] actor '%s' (%s) actorLoc=%s"),
			*DiagActor->GetName(), *DiagActor->GetClass()->GetName(), *DiagActor->GetActorLocation().ToString());
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
