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
	StaticMeshComp->SetSimulatePhysics(true);
	StaticMeshComp->SetEnableGravity(true);
	// 기본 질량(메시 부피 기반 자동 산출)이 너무 가벼워서 캐릭터가 스치기만 해도 멀리 날아가고,
	// 감쇠가 거의 없어 튕겨나간 속도로 바닥을 뚫고 지나가 사라진다 - 무게를 실어주고 감쇠를 걸고,
	// 고속 충돌에도 바닥을 통과하지 않게 CCD를 켠다.
	StaticMeshComp->SetMassOverrideInKg(NAME_None, 2.0f, true);
	StaticMeshComp->SetLinearDamping(1.0f);
	StaticMeshComp->SetAngularDamping(2.0f);
	StaticMeshComp->SetUseCCD(true);

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
}

void ADropItem::BeginPlay()
{
	Super::BeginPlay();

	InteractBox->OnComponentBeginOverlap.AddDynamic(this, &ADropItem::OnInteractBeginOverlap);
	InteractBox->OnComponentEndOverlap.AddDynamic(this, &ADropItem::OnInteractEndOverlap);
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

	/*-------------------
	 네트워킹: 아이템 습득 브로드캐스트
	-------------------*/
	if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
		{
			NetClient->SendInteractLoot(static_cast<int32>(GetUniqueID()));
		}
	}

	const int32 CountToAdd = FMath::Max(1, StackCount);
	int32 AddedCount = 0;
	for (; AddedCount < CountToAdd; ++AddedCount)
	{
		if (!InPlayer->GetInventoryComponent()->AddItem(ItemData)) break;
	}

	if (AddedCount > 0)
	{
		InPlayer->PlayPickupAnimationIfUnarmed();
	}

	if (AddedCount >= CountToAdd)
	{
		Destroy();
	}
	else if (AddedCount > 0)
	{
		// 인벤토리 공간이 모자라 일부만 주웠으면 남은 수량만큼 드롭 아이템을 그대로 남겨둔다.
		StackCount = CountToAdd - AddedCount;
	}
	// AddedCount == 0이면 인벤토리가 꽉 차서 하나도 못 주운 것 - 그대로 둔다.
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
