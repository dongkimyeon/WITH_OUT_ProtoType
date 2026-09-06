// Fill out your copyright notice in the Description page of Project Settings.

#include "CompanionNPC.h"
#include "CompanionListenComponent.h"
#include "CompanionBrainComponent.h"
#include "CompanionSpeechComponent.h"
#include "CompanionAIComponent.h"
#include "CompanionCombatComponent.h"
#include "CompanionPerceptionComponent.h"
#include "CompanionCommandRouterComponent.h"
#include "CompanionReportComponent.h"
#include "AIController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/weapon/WeaponBase.h"
#include "../Network/ProtoNetClientSubsystem.h"

ACompanionNPC::ACompanionNPC()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	ListenComponent = CreateDefaultSubobject<UCompanionListenComponent>(TEXT("ListenComponent"));
	BrainComponent = CreateDefaultSubobject<UCompanionBrainComponent>(TEXT("BrainComponent"));
	SpeechComponent = CreateDefaultSubobject<UCompanionSpeechComponent>(TEXT("SpeechComponent"));

	AIComponent = CreateDefaultSubobject<UCompanionAIComponent>(TEXT("AIComponent"));
	CombatComponent = CreateDefaultSubobject<UCompanionCombatComponent>(TEXT("CombatComponent"));
	PerceptionComponent = CreateDefaultSubobject<UCompanionPerceptionComponent>(TEXT("PerceptionComponent"));
	CommandRouterComponent = CreateDefaultSubobject<UCompanionCommandRouterComponent>(TEXT("CommandRouterComponent"));
	ReportComponent = CreateDefaultSubobject<UCompanionReportComponent>(TEXT("ReportComponent"));

	VisionCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("VisionCaptureComponent"));
	VisionCaptureComponent->SetupAttachment(RootComponent);
	VisionCaptureComponent->bCaptureEveryFrame = false;
	VisionCaptureComponent->bCaptureOnMovement = false;
	VisionCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	InventoryComponent = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("InventoryComponent"));
}

void ACompanionNPC::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsRemotePuppet)
	{
		// No CombatComponent/AIComponent/Controller left to compute these
		// from (MarkAsRemotePuppet destroyed/never-possessed them) -- feed
		// in whatever the owner last reported instead (see
		// UProtoNetClientSubsystem::UpdateRemoteCompanion). Same
		// CurrentWeaponType/bIsAiming/AimPitch properties the Animation
		// Blueprint already reads for a real, locally-owned companion, so
		// no ABP changes are needed for this to render correctly.
		bHasWeapon = MirroredWeaponType != EWeaponType::None;
		CurrentWeaponType = MirroredWeaponType;
		bIsAiming = bHasWeapon && bMirroredIsAiming;
		AimPitch = MirroredAimPitch;
		// Joint/LeftHandTransform (left-hand IK) and bIsReloading/
		// bIsSprint aren't synced -- known gap, see UpdateRemoteCompanion's
		// comment. Left at whatever they last were (their own defaults,
		// for a freshly-spawned puppet) rather than recomputed from local
		// components that don't exist here.
		return;
	}

	AWeaponBase* EquippedWeapon = CombatComponent ? CombatComponent->GetEquippedWeapon() : nullptr;
	bHasWeapon = IsValid(EquippedWeapon);
	CurrentWeaponType = bHasWeapon ? EquippedWeapon->WeaponType : EWeaponType::None;
	Joint = bHasWeapon && CombatComponent ? CombatComponent->GetLeftHandJointTargetForEquippedWeapon() : FVector(1000.0f, -2000.0f, 0.0f);

	bIsReloading = CombatComponent && CombatComponent->IsReloadingWeapon();
	SwappingAlpha = bHasWeapon && CombatComponent && CombatComponent->ShouldUseLeftHandIK();
	bIsAiming = bHasWeapon && AIComponent && AIComponent->IsAimingRequested();
	bIsSprint = AIComponent && AIComponent->ShouldSprintWhileFollowing();

	if (Controller)
	{
		const float NormalizedPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
		AimPitch = FMath::Clamp(NormalizedPitch, -30.0f, 30.0f);
	}
	else
	{
		AimPitch = 0.0f;
	}

	if (bHasWeapon && SwappingAlpha && GetMesh())
	{
		static const FName RightHandBoneName(TEXT("hand_r"));

		FTransform LeftHandSocketTransform;
		if (EquippedWeapon->GetLeftHandSocketTransform(LeftHandSocketTransform))
		{
			FVector OutPosition;
			FRotator OutRotation;
			GetMesh()->TransformToBoneSpace(
				RightHandBoneName,
				LeftHandSocketTransform.GetLocation(),
				LeftHandSocketTransform.Rotator(),
				OutPosition,
				OutRotation);

			LeftHandTransform = FTransform(OutRotation, OutPosition, FVector::OneVector);
		}
	}

	// 다른 플레이어 화면에도 이 동료가 보이도록, 위치/시선을 주기적으로 서버에 보고한다
	// (원격 플레이어를 따라다니는 동료는 없으므로 로컬 소유 동료일 때만 보낸다).
	if (bOwnedByLocalPlayer)
	{
		NetSyncTimer -= DeltaSeconds;
		if (NetSyncTimer <= 0.0f)
		{
			NetSyncTimer = NetSyncInterval;
			if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
			{
				if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
				{
					// CombatComponent is only ever null here for a remote
					// puppet, which never reaches this branch
					// (bOwnedByLocalPlayer is false for those).
					const float Health = CombatComponent ? CombatComponent->CurrentHealth : MirroredHealth;
					const bool bDead = CombatComponent && CombatComponent->IsDead();
					NetClient->SendCompanionMoveInput(GetActorLocation(), GetActorRotation(), Health, bDead,
						static_cast<uint8>(CurrentWeaponType), bIsAiming, AimPitch);
				}
			}
		}
	}
}
void ACompanionNPC::BeginPlay()
{
	if (bIsRemotePuppet)
	{
		// Super::BeginPlay() 전에 파이프라인 컴포넌트를 전부 destroy해서, 그
		// 컴포넌트들 자신의 BeginPlay()/Tick()조차 절대 돌지 않게 한다 --
		// Tick()의 나머지 로직은 이 컴포넌트들이 전부 nullptr이어도 이미
		// null 체크로 안전하게 no-op된다 (bHasWeapon=false 등 기본값으로).
		if (ListenComponent) { ListenComponent->DestroyComponent(); ListenComponent = nullptr; }
		if (BrainComponent) { BrainComponent->DestroyComponent(); BrainComponent = nullptr; }
		if (SpeechComponent) { SpeechComponent->DestroyComponent(); SpeechComponent = nullptr; }
		if (AIComponent) { AIComponent->DestroyComponent(); AIComponent = nullptr; }
		if (CombatComponent) { CombatComponent->DestroyComponent(); CombatComponent = nullptr; }
		if (PerceptionComponent) { PerceptionComponent->DestroyComponent(); PerceptionComponent = nullptr; }
		if (CommandRouterComponent) { CommandRouterComponent->DestroyComponent(); CommandRouterComponent = nullptr; }
		if (ReportComponent) { ReportComponent->DestroyComponent(); ReportComponent = nullptr; }
		if (VisionCaptureComponent) { VisionCaptureComponent->DestroyComponent(); VisionCaptureComponent = nullptr; }
		if (InventoryComponent) { InventoryComponent->DestroyComponent(); InventoryComponent = nullptr; }

		Super::BeginPlay();
		return;
	}

	Super::BeginPlay();

	// 다른 동료/적을 부드럽게 피해 돌아가도록 RVO 회피를 켠다. RVO는 등록된 에이전트끼리만
	// 상호 회피하므로 플레이어(비등록)는 피하지 못한다 - 플레이어가 좁은 통로를 완전히 막는
	// 경우는 UCompanionAIComponent의 스턱 감지가 명령을 포기시키는 방식으로 처리한다.
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->bUseRVOAvoidance = true;
		Movement->AvoidanceConsiderationRadius = 250.0f;
		Movement->AvoidanceWeight = 0.5f;
	}

	// 명령/대화 라우팅: 인식된 텍스트는 항상 Router를 먼저 거친다(키워드 즉시 실행 -> Brain 폴백).
	ListenComponent->OnTranscribed.AddDynamic(CommandRouterComponent, &UCompanionCommandRouterComponent::HandleTranscribed);

	// 대화 응답(순수 텍스트)은 곧바로 TTS로. 액션(Function Calling)은 Router가 받아 AIComponent에 배선.
	BrainComponent->OnReplyReady.AddDynamic(SpeechComponent, &UCompanionSpeechComponent::Speak);
	BrainComponent->OnActionRequested.AddDynamic(CommandRouterComponent, &UCompanionCommandRouterComponent::HandleActionRequested);

	// 무기는 인벤토리 내용에 따라 결정된다(있으면 장착, 없으면 맨손) - 스폰 시점 1회 + 이후 인벤토리가
	// 바뀔 때마다(탐색으로 줍거나 버릴 때) 재평가.
	CombatComponent->EquipWeaponFromInventory(InventoryComponent);
	InventoryComponent->OnInventoryChanged.AddDynamic(this, &ACompanionNPC::HandleInventoryChanged);
}

void ACompanionNPC::MarkAsRemotePuppet()
{
	bIsRemotePuppet = true;
	AutoPossessAI = EAutoPossessAI::Disabled;
}

void ACompanionNPC::HandleInventoryChanged()
{
	CombatComponent->EquipWeaponFromInventory(InventoryComponent);
}

void ACompanionNPC::SetOwningPlayer(APawn* Player)
{
	if (AIComponent)
	{
		AIComponent->SetFollowTarget(Player);
	}

	bOwnedByLocalPlayer = Player && Player->IsLocallyControlled();
}


