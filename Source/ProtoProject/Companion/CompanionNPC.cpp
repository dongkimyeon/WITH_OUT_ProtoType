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
#include "Components/SceneCaptureComponent2D.h"
#include "../PlayerContent/Inventory/InventoryGridComponent.h"
#include "../PlayerContent/weapon/WeaponBase.h"

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

	AWeaponBase* EquippedWeapon = CombatComponent ? CombatComponent->GetEquippedWeapon() : nullptr;
	bHasWeapon = IsValid(EquippedWeapon);
	CurrentWeaponType = bHasWeapon ? EquippedWeapon->WeaponType : EWeaponType::None;
	Joint = bHasWeapon ? EquippedWeapon->LeftHandJointTarget : FVector(1000.0f, -2000.0f, 0.0f);

	bIsAiming = bHasWeapon && AIComponent && AIComponent->IsAimingRequested();
	SwappingAlpha = bHasWeapon && !bIsReloading;
	bIsSprint = false;

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
}
void ACompanionNPC::BeginPlay()
{
	Super::BeginPlay();

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
}
