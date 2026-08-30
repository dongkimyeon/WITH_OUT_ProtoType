#include "ItemSpawnPoint.h"
#include "DropItem.h"
#include "LootTierRoll.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"

AItemSpawnPoint::AItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;

	RangeViz = CreateDefaultSubobject<USphereComponent>(TEXT("RangeViz"));
	RangeViz->SetupAttachment(RootComponent);
	RangeViz->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RangeViz->SetCanEverAffectNavigation(false);
	RangeViz->SetHiddenInGame(true);
	RangeViz->ShapeColor = FColor(255, 200, 0);
	RangeViz->SetSphereRadius(ScatterRadius);
}

void AItemSpawnPoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// 에디터에서 ScatterRadius를 바꾸면 시각화 구도 즉시 따라가게 한다.
	RangeViz->SetSphereRadius(FMath::Max(ScatterRadius, 1.f));
}

void AItemSpawnPoint::BeginPlay()
{
	Super::BeginPlay();

	if (bSpawnOnBeginPlay)
	{
		SpawnLoot();
	}
}

void AItemSpawnPoint::SpawnLoot()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const int32 Count = FMath::RandRange(FMath::Min(MinItems, MaxItems), FMath::Max(MinItems, MaxItems));
	for (int32 i = 0; i < Count; ++i)
	{
		UItemDataBase* Chosen = LootTier::RollItem(RegionTier);
		if (!Chosen)
		{
			continue;
		}

		FVector Offset = FVector(FMath::VRand().GetSafeNormal2D() * FMath::FRand() * ScatterRadius);
		Offset.Z = 30.f;

		// 스폰 포인트 액터의 스케일이 아이템에 새어들지 않도록 위치/회전만 쓰고 스케일은 1로 고정.
		const FTransform SpawnTransform(GetActorQuat(), GetActorLocation() + Offset, FVector(1.f));

		// ItemData는 OnConstruction이 메시를 붙이는 데 쓰이므로 Deferred 스폰으로 먼저 세팅한다.
		// (AEnemyBase::SpawnLoot와 동일한 패턴)
		ADropItem* Drop = World->SpawnActorDeferred<ADropItem>(ADropItem::StaticClass(), SpawnTransform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Drop)
		{
			continue;
		}

		Drop->ItemData = Chosen;
		Drop->StackCount = 1;
		Drop->NetSlotId = GetTypeHash(GetName() + FString::FromInt(i));
		Drop->FinishSpawning(SpawnTransform);
	}
}
