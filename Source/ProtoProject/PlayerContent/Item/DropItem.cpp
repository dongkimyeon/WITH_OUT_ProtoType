#include "DropItem.h"
#include "../ProtoCharacter.h"
#include "../Inventory/InventoryGridComponent.h"
#include "Engine/GameInstance.h"
#include "../../Network/ProtoNetClientSubsystem.h"

ADropItem::ADropItem()
{
	PrimaryActorTick.bCanEverTick = false;

	StaticMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComp"));
	RootComponent = StaticMeshComp;
	// 드롭 아이템(특히 진열대/상자 위에 얹힌 것들) 콜리전이 네브메시 생성에 끼면 바닥과 연결 안 된
	// 고립된 네브메시 조각이 생겨서 AI가 경로를 못 찾는다 - 아이템은 네비게이션에서 완전히 제외한다.
	StaticMeshComp->SetCanEverAffectNavigation(false);

	// 물리 시뮬레이션: 떨어뜨리면 중력에 반응해 바닥에 안착한다. 월드(바닥/벽/다른 물리 오브젝트)와는
	// 부딪히되, 플레이어/컴패니언(Pawn)은 그대로 통과해서 걸리적거리지 않게 한다.
	StaticMeshComp->SetMobility(EComponentMobility::Movable);
	StaticMeshComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	StaticMeshComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	// 물리 시뮬레이션 활성화와 질량 오버라이드는 BeginPlay에서 한다. 생성자(특히 CDO 생성 시점)에서
	// 호출하면 GEngine 초기화 전이라 FBodyInstance::GetSimplePhysicalMaterial 이 LogPhysics:Error 를
	// 찍고, 이 에러가 쿡 커맨드렛 종료 코드를 1로 만들어 패키징이 실패한다.

	BoundingBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundingBox"));
	BoundingBox->SetupAttachment(StaticMeshComp);
	BoundingBox->SetBoxExtent(FVector(30.f, 30.f, 30.f));
	BoundingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	BoundingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BoundingBox->SetCanEverAffectNavigation(false);

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(StaticMeshComp);
	InteractBox->SetBoxExtent(FVector(150.f, 150.f, 150.f));
	InteractBox->SetCollisionProfileName(TEXT("Trigger"));
	InteractBox->SetCanEverAffectNavigation(false);
}

void ADropItem::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (!StaticMeshComp) return;

	if (ItemData && !ItemData->ItemMesh.IsNull())
	{
		StaticMeshComp->SetStaticMesh(ItemData->ItemMesh.LoadSynchronous());
	}
	else
	{
		StaticMeshComp->SetStaticMesh(nullptr);
	}

	// WorldMeshScale이 기본값(1,1,1)이 아니면 적용. 기본값이면 안 건드려서
	// BP_DropItem_* 를 직접 배치한 경우의 스케일 오버라이드를 그대로 둔다.
	if (ItemData && !ItemData->WorldMeshScale.Equals(FVector::OneVector))
	{
		StaticMeshComp->SetRelativeScale3D(ItemData->WorldMeshScale);
	}
}

void ADropItem::BeginPlay()
{
	Super::BeginPlay();

	// 떨어뜨리면 중력에 반응해 바닥에 안착한다. 기본 질량(메시 부피 기반 자동 산출)이 너무 가벼워서
	// 캐릭터가 스치기만 해도 멀리 날아가고, 감쇠가 거의 없어 튕겨나간 속도로 바닥을 뚫고 지나가 사라진다
	// - 무게를 실어주고 감쇠를 걸고, 고속 충돌에도 바닥을 통과하지 않게 CCD를 켠다.
	StaticMeshComp->SetSimulatePhysics(true);
	StaticMeshComp->SetEnableGravity(true);
	StaticMeshComp->SetMassOverrideInKg(NAME_None, 2.0f, true);
	StaticMeshComp->SetLinearDamping(1.0f);
	StaticMeshComp->SetAngularDamping(2.0f);
	StaticMeshComp->SetUseCCD(true);

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ADropItem::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ADropItem::OnInteractEndOverlap);

	// 문제: "다른 플레이어가 먹은 아이템이 유령 상태로 존재하다가 상호작용하면
	// 그제서야 없어짐" -- 예전엔 이 델리게이트를 RequestPickup() 안에서만
	// 바인딩해서, 이 클라이언트가 직접 주우려고 시도하기 전까진 이 아이템의
	// OnItemPickupResult를 아예 안 듣고 있었다. 그래서 다른 플레이어가 먼저
	// 주워서 서버가 S2C_InteractResult{Ok}를 브로드캐스트해도 이 클라이언트는
	// 그걸 놓치고, 로컬 플레이어가 나중에 이 아이템을 직접 상호작용해야만(그때야
	// 비로소 바인딩되고, 이미 클레임됐으니 Denied를 받아) 사라졌다. 이제
	// BeginPlay에서 항상 바인딩해서, 내가 직접 주우려 하지 않아도 다른 사람이
	// 먼저 가져가는 순간 HandlePickupResult(bGranted=true, 남의 PickerPlayerId)가
	// 와서 즉시 파괴되게 한다 -- RequestPickup()은 더 이상 따로 바인딩할 필요
	// 없음(중복 바인딩 방지를 위해 그쪽 호출은 제거).
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->OnItemPickupResult.AddUniqueDynamic(this, &ADropItem::HandlePickupResult);
		}
	}
}

void ADropItem::OnInteractBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32, bool, const FHitResult&)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableEnter(this);
}

void ADropItem::OnInteractEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
	UPrimitiveComponent*, int32)
{
	AProtoCharacter* Player = Cast<AProtoCharacter>(OtherActor);
	if (Player) Player->OnInteractableExit(this);
}

void ADropItem::OnInteract_Implementation(AProtoCharacter* InPlayer)
{
	if (!InPlayer || !ItemData) return;

	RequestPickup(InPlayer->GetInventoryComponent(), InPlayer);
}

void ADropItem::RequestPickup(UInventoryGridComponent* TargetInventory, AProtoCharacter* PickupAnimPlayer)
{
	if (!TargetInventory || !ItemData || bPickupRequested)
	{
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;

	// NetSlotId == 0 means whatever spawned this drop never assigned it a
	// stable, cross-client id (e.g. AEnemyBase::SpawnLoot's death drops --
	// their contents aren't rolled through the server at all yet, a
	// separate, bigger gap than this fix covers). Arbitrating pickup on an
	// id shared by every such untagged drop in the level would permanently
	// lock out every OTHER untagged drop the instant the first one is ever
	// claimed -- treat it the same as "no one else to desync with" instead.
	if (!NetClient || !NetClient->IsConnected() || !NetClient->IsMultiplayerVisualsEnabled() || NetSlotId == 0)
	{
		// Pick it up immediately, same as before this feature existed. A
		// server round trip that will never arrive (not connected, or a Solo
		// map where SendInteractLoot no-ops on the bMultiplayerVisualsEnabled
		// gate) or that would collide with every other untagged drop
		// (NetSlotId == 0) would otherwise mean the item can never be picked
		// up correctly -- in the Solo case bPickupRequested would latch true
		// forever with no reply coming, permanently locking out that item.
		ResolvePickup(TargetInventory, PickupAnimPlayer, /*bGrantedToMe=*/true);
		return;
	}

	// Ask the server first: two clients could be interacting with this same
	// synced ground item (spawned from the same S2C_ItemSpawnState/
	// S2C_ContainerLootState roll) in the same instant, and adding it to an
	// inventory before the server arbitrates would duplicate it. See
	// OnItemPickupResult's schema comment for what happens next.
	bPickupRequested = true;
	PendingTargetInventory = TargetInventory;
	PendingPickupAnimPlayer = PickupAnimPlayer;
	// BeginPlay()가 이미 OnItemPickupResult를 바인딩해뒀다(위 주석 참고) --
	// 여기서 또 바인딩하면 AddDynamic(중복 허용)이라 두 번 불려서
	// HandlePickupResult가 같은 결과에 두 번 반응하게 된다.
	NetClient->SendInteractLoot(NetSlotId);
}

void ADropItem::HandlePickupResult(int32 ResolvedNetSlotId, int32 PickerPlayerId, bool bGranted)
{
	if (ResolvedNetSlotId != NetSlotId)
	{
		// Every ADropItem in the level shares this one broadcast delegate --
		// not our answer.
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (NetClient)
	{
		// Ok or Denied, this NetSlotId's outcome is final either way --
		// safe to stop listening now.
		NetClient->OnItemPickupResult.RemoveDynamic(this, &ADropItem::HandlePickupResult);
	}

	// bGranted false (Denied) means this was never a race to begin with --
	// bGrantedToMe short-circuits to false without even checking PickerPlayerId.
	const bool bGrantedToMe = bGranted && NetClient && PickerPlayerId == NetClient->GetLocalPlayerId();
	ResolvePickup(PendingTargetInventory.Get(), PendingPickupAnimPlayer.Get(), bGrantedToMe);
}

void ADropItem::ResolvePickup(UInventoryGridComponent* TargetInventory, AProtoCharacter* PickupAnimPlayer, bool bGrantedToMe)
{
	if (!bGrantedToMe)
	{
		// Another client's request won the race for this exact item -- it's
		// gone from the world either way, just not into OUR inventory.
		Destroy();
		return;
	}

	if (!TargetInventory)
	{
		// Whoever wanted it (player or companion) is gone by the time the
		// server answered -- nothing left to give it to. Leave the item be
		// rather than silently deleting it into the void; the server
		// already marked this NetSlotId claimed, so no one else can pick it
		// up either, but that's a rare enough edge case (target destroyed
		// mid-request) not to warrant more than that.
		return;
	}

	const int32 CountToAdd = FMath::Max(1, StackCount);
	int32 AddedCount = 0;
	for (; AddedCount < CountToAdd; ++AddedCount)
	{
		if (!TargetInventory->AddItem(ItemData)) break;
	}

	if (AddedCount > 0 && PickupAnimPlayer)
	{
		PickupAnimPlayer->PlayPickupAnimationIfUnarmed();
	}

	if (AddedCount >= CountToAdd)
	{
		Destroy();
	}
	else if (AddedCount > 0)
	{
		// Inventory ran out of room for the rest -- leave the remainder as
		// a drop item locally. Known residual edge case: the server already
		// considers this NetSlotId fully claimed (first-claim-wins isn't
		// "claim N units"), so every other client already destroyed their
		// copy of this item even though this client still shows a leftover
		// stack. Rare (requires a full inventory) and pre-existing (the
		// original single-player-only version had the same partial-pickup
		// behavior); not fixed here.
		StackCount = CountToAdd - AddedCount;
	}
	// AddedCount == 0 means the inventory was already full -- leave it be.
}

FText ADropItem::GetInteractPrompt_Implementation() const
{
	if (ItemData)
	{
		if (StackCount > 1)
		{
            return FText::Format(FText::FromString(TEXT("F  줍기  [{0} x{1}]")), ItemData->DisplayName, FText::AsNumber(StackCount));
		}
        return FText::Format(FText::FromString(TEXT("F  줍기  [{0}]")), ItemData->DisplayName);
	}
	return FText::FromString(TEXT("F  줍기"));
}

bool ADropItem::CanInteract_Implementation(AProtoCharacter* InPlayer) const
{
	return true;
}

void ADropItem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}
