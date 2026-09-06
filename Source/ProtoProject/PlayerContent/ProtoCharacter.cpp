#include "ProtoCharacter.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraShakeBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "EquipmentComponent.h"
#include "QuickSlotComponent.h"
#include "RadialQuickSlotWidget.h"
#include "Item/ConsumableItemData.h"
#include "ContainerScreenWidget.h"
#include "Item/ItemDataBase.h"
#include "Item/WeaponItemData.h"
#include "Item/ItemContainerBase.h"
#include "PlayerDefalutUI.h"
#include "PlayerStatusComponent.h"
#include "InputCoreTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Animation/AnimMontage.h"
#include "../Enemy/EnemyBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "weapon/WeaponBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "../LevelChange/LevelChanger.h"
#include "../LevelChange/LevelChangeSelectWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "../Network/ProtoNetClientSubsystem.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "../Companion/CompanionNPC.h"
#include "../Companion/CompanionAIComponent.h"
#include "../Companion/CompanionCombatComponent.h"
#include "../Companion/CompanionListenComponent.h"
#include "ProtoDebugPanel.h"
#include "HAL/IConsoleManager.h"
#include "Engine/GameViewportClient.h"

AProtoCharacter::AProtoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    InventoryComponent = CreateDefaultSubobject<UInventoryGridComponent>(TEXT("InventoryComponent"));
    EquipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("EquipmentComponent"));
    QuickSlotComponent = CreateDefaultSubobject<UQuickSlotComponent>(TEXT("QuickSlotComponent"));
    StatusComponent = CreateDefaultSubobject<UPlayerStatusComponent>(TEXT("StatusComponent"));
    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

    static ConstructorHelpers::FObjectFinder<UAnimMontage> RifleReloadMontageFinder(TEXT("/Game/Blueprint/AM_Player_Upper.AM_Player_Upper"));
    if (RifleReloadMontageFinder.Succeeded())
    {
        RifleReloadMontage = RifleReloadMontageFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UAnimSequenceBase> PickupAnimationFinder(TEXT("/Game/testasset/Picking_Up.Picking_Up"));
    if (PickupAnimationFinder.Succeeded())
    {
        PickupAnimation = PickupAnimationFinder.Object;
    }

    // Same Blueprints the inventory/equipment system spawns from -- see
    // SpawnFallbackRemoteWeapon()'s comment in the header for why a remote
    // character needs its own copy instead of reusing that system.
    static ConstructorHelpers::FClassFinder<AWeaponBase> RemoteRifleClassFinder(TEXT("/Game/Blueprint/weapon/BP_AK47"));
    if (RemoteRifleClassFinder.Succeeded())
    {
        RemoteRifleClass = RemoteRifleClassFinder.Class;
    }

    static ConstructorHelpers::FClassFinder<AWeaponBase> RemotePistolClassFinder(TEXT("/Game/Blueprint/weapon/Pistol"));
    if (RemotePistolClassFinder.Succeeded())
    {
        RemotePistolClass = RemotePistolClassFinder.Class;
    }

    // CompanionClass는 FClassFinder로 자동 채우지 않는다 - BP_CompanionNPC가 쓰는 애니메이션
    // 블루프린트(ABP_Unarmed_Test)가 BP_ProtoCharacter를 다시 참조하고 있어서, 여기서 동기 로드를
    // 걸면 BP_ProtoCharacter 생성 중에 자기 자신을 기다리는 순환 참조로 로딩이 멈춘다
    // (LoadingIsStuck 크래시). 대신 BP_ProtoCharacter 디테일 패널에서 Companion > CompanionClass에
    // BP_CompanionNPC를 직접 지정해야 한다.
}

// ProtoCharacter.h 참고: SProtoDebugPanel이 전방 선언뿐이라, DebugPanelWidget(TSharedPtr) 소멸을
// 이 TU(ProtoDebugPanel.h를 완전히 include한 곳)에서만 일어나게 하려고 일부러 여기서 정의한다.
AProtoCharacter::~AProtoCharacter() = default;

void AProtoCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // See TrySetupLocalPlayerOnce's header comment: BeginPlay already tried
    // this once, this just keeps retrying (no-ops instantly once done) in
    // case IsLocallyControlled() wasn't ready yet at that exact moment.
    if (!bLocalPlayerSetupDone)
    {
        TrySetupLocalPlayerOnce();
    }

    // 사망 후에는 래그돌 상태이므로 무기 IK/스왑/네트워크 위치 전송을 멈춘다.
    if (bIsDead)
    {
        return;
    }

    if (Controller)
    {
        const float NormalizedPitch = FRotator::NormalizeAxis(Controller->GetControlRotation().Pitch);
        AimPitch = FMath::Clamp(NormalizedPitch, -30.0f, 30.0f);
    }
    if (Swapping > 0.0f)
    {
        Swapping = FMath::Max(0.0f, Swapping - DeltaTime);
        SwappingAlpha = false;

        if (Swapping <= 0.0f)
        {
            FinishWeaponSwap();
        }
    }

    if (bHasWeapon && Swapping <= 0.0f && CurrentWeapon && GetMesh())
    {
        static const FName RightHandBoneName(TEXT("hand_r"));

        FTransform LeftHandSocketTransform;
        if (CurrentWeapon->GetLeftHandSocketTransform(LeftHandSocketTransform))
        {
            const FVector LeftHandWorldLocation = LeftHandSocketTransform.GetLocation();

            FVector OutPosition;
            FRotator OutRotation;
            GetMesh()->TransformToBoneSpace(
                RightHandBoneName,
                LeftHandWorldLocation,
                LeftHandSocketTransform.Rotator(),
                OutPosition,
                OutRotation);

            LeftHandTransform = FTransform(OutRotation, OutPosition, FVector::OneVector);

            }
        }
#if !(UE_BUILD_SHIPPING)
    if (IsLocallyControlled() && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            91002,
            0.0f,
            FColor::Cyan,
            FString::Printf(TEXT("SwappingAlpha: %s | Swapping: %.2f | WeaponType: %d"),
                SwappingAlpha ? TEXT("TRUE") : TEXT("FALSE"),
                Swapping,
                static_cast<int32>(CurrentWeaponType)));
    }
#endif
    UpdateStamina(DeltaTime);

    /*-------------------
     네트워킹: 위치 브로드캐스트 (NetSyncInterval 간격으로 주기 전송)
    -------------------*/
    if (IsLocallyControlled())
    {
        NetSyncTimer -= DeltaTime;
        if (NetSyncTimer <= 0.0f)
        {
            NetSyncTimer = NetSyncInterval;
            if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
            {
                if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
                {
                    // Sprint/ADS were never actually included here before --
                    // SendMoveInput() defaulted Flags to 0, so remote copies
                    // never got real sprint speed or aim state, only position.
                    int32 MoveFlags = 0;
                    if (bIsSprint) MoveFlags |= UProtoNetClientSubsystem::kMoveFlagSprint;
                    if (bIsAiming) MoveFlags |= UProtoNetClientSubsystem::kMoveFlagADS;
                    NetClient->SendMoveInput(GetActorLocation(), GetControlRotation(), MoveFlags);
                }
            }
        }
    }

    if (bIsReloading)
    {
        SwappingAlpha = false;
    }
}

void AProtoCharacter::BeginPlay()
{
    Super::BeginPlay();

    bDebugLeftHandIK = false;

    /*if (IsLocallyControlled())
    {
        if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
        {
            PlayerController->ConsoleCommand(TEXT("DisableAllScreenMessages"));
        }
    }*/

    StopAim();
    StopSprint();

    if (EquipmentComponent)
    {
        EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &AProtoCharacter::HandleEquipmentChanged);
    }

    /*-------------------
     네트워킹: 저장된 진행 상황 복원 구독 (로컬 플레이어만)
    -------------------*/
    // Tries immediately; if IsLocallyControlled() isn't ready yet at this
    // exact BeginPlay moment, Tick() keeps retrying until it succeeds --
    // see TrySetupLocalPlayerOnce's header comment.
    TrySetupLocalPlayerOnce();

    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
            if (CompanionMappingContext)
            {
                Subsystem->AddMappingContext(CompanionMappingContext, 0);
            }
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
        {
            AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AProtoCharacter::HandleMontageNotifyBegin);
            AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &AProtoCharacter::HandleMontageEnded);
        }
    }
    if (InventoryComponent)
    {
        /*if (TestArmor) InventoryComponent->AddItem(TestArmor);
        if (TestRifle) InventoryComponent->AddItem(TestRifle);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);
        if (TestBandage) InventoryComponent->AddItem(TestBandage);*/
    }

}

bool AProtoCharacter::TrySetupLocalPlayerOnce()
{
    if (bLocalPlayerSetupDone || !IsLocallyControlled())
    {
        return bLocalPlayerSetupDone;
    }

    if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->OnProgressRestored.AddDynamic(this, &AProtoCharacter::HandleProgressRestored);
            NetClient->OnInventoryRestored.AddDynamic(this, &AProtoCharacter::HandleInventoryRestored);
            NetClient->OnEnemyAttackPlayer.AddDynamic(this, &AProtoCharacter::HandleEnemyAttackPlayer);

            // Login via TitleLevel completes (S2C_LoginSuccess arrives,
            // OnProgressRestored/OnInventoryRestored fire) before this
            // character even exists -- it only spawns once
            // HandleLoginSucceeded's OpenLevelBySoftObjectPtr() finishes
            // loading this level, which is after the broadcasts above
            // already fired to nobody. Pull whatever was cached instead
            // of only relying on the (still correct for the old in-game
            // Slate popup flow) broadcast.
            FVector PendingRestorePosition;
            FRotator PendingRestoreLook;
            uint8 PendingRestoreWeaponType = 0;
            if (NetClient->ConsumePendingProgressRestore(PendingRestorePosition, PendingRestoreLook, PendingRestoreWeaponType))
            {
                HandleProgressRestored(PendingRestorePosition, PendingRestoreLook, PendingRestoreWeaponType);
            }

            TArray<FProtoInventoryItemEntry> PendingInventory;
            TArray<FProtoEquipmentEntry> PendingEquipment;
            TArray<FProtoQuickSlotEntry> PendingQuickSlots;
            if (NetClient->ConsumePendingInventoryRestore(PendingInventory, PendingEquipment, PendingQuickSlots))
            {
                HandleInventoryRestored(PendingInventory);
                RestoreEquipmentAndQuickSlots(PendingEquipment, PendingQuickSlots);
            }
        }
    }

    if (InventoryComponent)
    {
        InventoryComponent->OnInventoryChanged.AddDynamic(this, &AProtoCharacter::HandleInventoryChanged);
    }

    if (EquipmentComponent)
    {
        EquipmentComponent->OnEquipmentChanged.AddDynamic(this, &AProtoCharacter::HandleEquipmentChangedForSave);
    }

    if (QuickSlotComponent)
    {
        QuickSlotComponent->OnQuickSlotChanged.AddDynamic(this, &AProtoCharacter::HandleQuickSlotChangedForSave);
    }

    if (StatusComponent)
    {
        StatusComponent->OnPlayerDied.AddDynamic(this, &AProtoCharacter::HandleDeath);
    }

    SpawnCompanion();

    // Only the locally-controlled player's own HUD should go on screen; this
    // BeginPlay also runs for remote players spawned by
    // UProtoNetClientSubsystem (see ProtoNetClientSubsystem.cpp), which have
    // no local controller (and so never reach here at all, per the early
    // IsLocallyControlled() check above).
    if (DefaultUIClass)
    {
        DefaultUI = CreateWidget<UPlayerDefalutUI>(GetWorld(), DefaultUIClass);
        if (DefaultUI)
        {
            DefaultUI->AddToViewport();

            if (CachedCompanionNPC)
            {
                DefaultUI->AddCompanionStatusLabel(CachedCompanionNPC);
            }
        }
    }

    bLocalPlayerSetupDone = true;
    return true;
}

void AProtoCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Move);
        EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProtoCharacter::Look);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
        EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AProtoCharacter::Sprint);
        EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AProtoCharacter::ToggleInventory);

        if (InteractAction)
        {
            EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AProtoCharacter::Interact);
        }

        if (TalkToCompanionAction)
        {
            EnhancedInputComponent->BindAction(TalkToCompanionAction, ETriggerEvent::Started, this, &AProtoCharacter::TalkToCompanionPressed);
            EnhancedInputComponent->BindAction(TalkToCompanionAction, ETriggerEvent::Completed, this, &AProtoCharacter::TalkToCompanionReleased);
            EnhancedInputComponent->BindAction(TalkToCompanionAction, ETriggerEvent::Canceled, this, &AProtoCharacter::TalkToCompanionReleased);
        }
    }

    PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AProtoCharacter::SetWeaponSlot1);
    PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AProtoCharacter::SetWeaponSlot2);
    PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AProtoCharacter::SetWeaponTypeNone);
    PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AProtoCharacter::OnQuickSlotKeyPressed);
    PlayerInputComponent->BindKey(EKeys::Four, IE_Released, this, &AProtoCharacter::OnQuickSlotKeyReleased);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AProtoCharacter::StartFireWeapon);
    PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AProtoCharacter::StopFireWeapon);
    PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AProtoCharacter::ReloadWeapon);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AProtoCharacter::StartSprint);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AProtoCharacter::StopSprint);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AProtoCharacter::StartAim);
    PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AProtoCharacter::StopAim);

#if !UE_BUILD_SHIPPING
    // 이전엔 5/6/7/8/9/0/-/[ 숫자키에 흩어져 있던 디버그 명령들 - 전부 0번 키로 여는 디버그
    // 패널(ProtoDebugPanel) 버튼으로 옮겼다. 진입점을 하나로 모아 뭘 눌러야 할지 안 외워도 되게.
    PlayerInputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AProtoCharacter::ToggleDebugPanel);
    PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AProtoCharacter::ToggleEnemySoundsDebug);
#endif
}

void AProtoCharacter::ToggleEnemySoundsDebug()
{
    AEnemyBase::ToggleEnemySoundsEnabled();

    if (GEngine)
    {
        const bool bEnabled = AEnemyBase::AreEnemySoundsEnabled();
        GEngine->AddOnScreenDebugMessage(94001, 2.0f, bEnabled ? FColor::Green : FColor::Red,
            bEnabled ? TEXT("Zombie Sounds: ON") : TEXT("Zombie Sounds: OFF"));
    }
}
void AProtoCharacter::DebugCommandCompanionEngage()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    if (!Companion || !Companion->AIComponent)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(93002, 2.0f, FColor::Red, TEXT("Companion engage failed: no companion or AI component"));
        }
        return;
    }

    Companion->AIComponent->CommandEngage();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93002, 2.0f, FColor::Green, TEXT("Companion command: Engage"));
    }
}
void AProtoCharacter::DebugCommandCompanionExplore()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    if (!Companion || !Companion->AIComponent)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(93001, 2.0f, FColor::Red, TEXT("Companion explore failed: no companion or AI component"));
        }
        return;
    }

    Companion->AIComponent->CommandExplore();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93001, 2.0f, FColor::Green, TEXT("Companion command: Explore"));
    }
}
void AProtoCharacter::DebugCommandCompanionEquipWeapon1()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    const bool bSucceeded = Companion && Companion->CombatComponent && Companion->CombatComponent->EquipWeaponFromInventoryIndex(Companion->InventoryComponent, 0);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93003, 2.0f, bSucceeded ? FColor::Green : FColor::Red,
            bSucceeded ? TEXT("Companion equip weapon 1") : TEXT("Companion equip weapon 1 failed"));
    }
}

void AProtoCharacter::DebugCommandCompanionEquipWeapon2()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    const bool bSucceeded = Companion && Companion->CombatComponent && Companion->CombatComponent->EquipWeaponFromInventoryIndex(Companion->InventoryComponent, 1);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93004, 2.0f, bSucceeded ? FColor::Green : FColor::Red,
            bSucceeded ? TEXT("Companion equip weapon 2") : TEXT("Companion equip weapon 2 failed"));
    }
}

void AProtoCharacter::DebugCommandCompanionHolsterWeapon()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    if (!Companion || !Companion->CombatComponent)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(93005, 2.0f, FColor::Red, TEXT("Companion holster failed"));
        }
        return;
    }

    Companion->CombatComponent->HolsterWeapon();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93005, 2.0f, FColor::Green, TEXT("Companion holster weapon"));
    }
}

void AProtoCharacter::DebugCommandCompanionJump()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    if (!Companion)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(93006, 2.0f, FColor::Red, TEXT("Companion jump failed"));
        }
        return;
    }

    Companion->Jump();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93006, 2.0f, FColor::Green, TEXT("Companion jump"));
    }
}

void AProtoCharacter::DebugCommandCompanionReload()
{
    ACompanionNPC* Companion = GetCompanionNPC();
    if (!Companion || !Companion->CombatComponent)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(93007, 2.0f, FColor::Red, TEXT("Companion reload failed"));
        }
        return;
    }

    Companion->CombatComponent->ReloadWeapon();
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93007, 2.0f, FColor::Green, TEXT("Companion reload"));
    }
}
void AProtoCharacter::DebugDecreaseHealth()
{
    if (StatusComponent) StatusComponent->SetHealth(StatusComponent->GetHealth() - 5.0f);
}

void AProtoCharacter::DebugDecreaseHunger()
{
    if (StatusComponent) StatusComponent->SetHunger(StatusComponent->GetHunger() - 5.0f);
}

void AProtoCharacter::DebugDecreaseThirst()
{
    if (StatusComponent) StatusComponent->SetThirst(StatusComponent->GetThirst() - 5.0f);
}

void AProtoCharacter::DebugIncreaseInfection()
{
    if (StatusComponent) StatusComponent->SetInfection(StatusComponent->GetInfection() + 5.0f);
}

void AProtoCharacter::DebugDecreaseStamina()
{
    if (StatusComponent) StatusComponent->SetStamina(StatusComponent->GetStamina() - 5.0f);
}

void AProtoCharacter::ToggleDebugPanel()
{
#if !UE_BUILD_SHIPPING
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC)
    {
        return;
    }

    // 이미 열려 있으면 닫기.
    if (DebugPanelWidget.IsValid())
    {
        if (GEngine && GEngine->GameViewport)
        {
            GEngine->GameViewport->RemoveViewportWidgetContent(DebugPanelWidget.ToSharedRef());
        }
        DebugPanelWidget.Reset();

        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
        return;
    }

    // 패널이 액션을 몰라도 되게, 실제 어떤 함수를 부를지는 여기서 구성해 넘긴다(ProtoDebugPanel.h 참고).
    TArray<FProtoDebugSection> Sections;

    FProtoDebugSection PlayerSection;
    PlayerSection.Title = FText::FromString(TEXT("플레이어 상태"));
    PlayerSection.Actions.Add({ FText::FromString(TEXT("체력 -5")), [this]() { DebugDecreaseHealth(); } });
    PlayerSection.Actions.Add({ FText::FromString(TEXT("배고픔 -5")), [this]() { DebugDecreaseHunger(); } });
    PlayerSection.Actions.Add({ FText::FromString(TEXT("갈증 -5")), [this]() { DebugDecreaseThirst(); } });
    PlayerSection.Actions.Add({ FText::FromString(TEXT("감염 +5")), [this]() { DebugIncreaseInfection(); } });
    PlayerSection.Actions.Add({ FText::FromString(TEXT("스태미나 -5")), [this]() { DebugDecreaseStamina(); } });
    Sections.Add(PlayerSection);

    FProtoDebugSection CompanionSection;
    CompanionSection.Title = FText::FromString(TEXT("동료 명령"));
    CompanionSection.Actions.Add({ FText::FromString(TEXT("교전")), [this]() { DebugCommandCompanionEngage(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("탐색")), [this]() { DebugCommandCompanionExplore(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("무기1 장착")), [this]() { DebugCommandCompanionEquipWeapon1(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("무기2 장착")), [this]() { DebugCommandCompanionEquipWeapon2(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("무기 해제")), [this]() { DebugCommandCompanionHolsterWeapon(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("재장전")), [this]() { DebugCommandCompanionReload(); } });
    CompanionSection.Actions.Add({ FText::FromString(TEXT("점프")), [this]() { DebugCommandCompanionJump(); } });
    Sections.Add(CompanionSection);

    FProtoDebugSection TuningSection;
    TuningSection.Title = FText::FromString(TEXT("동료 튜닝 (companion.* CVar, PIE 실시간 반영)"));

    // companion.FollowDistance/SightRadius/AttackRange는 기본값 -1(오버라이드 없음, 인스턴스
    // 프로퍼티 그대로 사용)이라 슬라이더를 조금이라도 움직이면 즉시 그 값으로 덮어써진다 - CVar
    // 자체가 원래 그런 "오버라이드 스위치"라 패널에서도 동일하게 동작한다.
    auto AddCVarSlider = [&TuningSection](const TCHAR* CVarName, const FText& Label, float MinV, float MaxV)
    {
        IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
        if (!CVar)
        {
            return;
        }
        FProtoDebugSlider Slider;
        Slider.Label = Label;
        Slider.MinValue = MinV;
        Slider.MaxValue = MaxV;
        Slider.GetValue = [CVar]() { return CVar->GetFloat(); };
        Slider.SetValue = [CVar](float NewValue) { CVar->Set(NewValue); };
        TuningSection.Sliders.Add(Slider);
    };

    AddCVarSlider(TEXT("companion.FollowDistance"), FText::FromString(TEXT("FollowDistance")), 0.0f, 2000.0f);
    AddCVarSlider(TEXT("companion.SightRadius"), FText::FromString(TEXT("SightRadius")), 0.0f, 6000.0f);
    AddCVarSlider(TEXT("companion.AttackRange"), FText::FromString(TEXT("AttackRange")), 0.0f, 4000.0f);

    if (IConsoleVariable* DebugDrawCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("companion.DebugDraw")))
    {
        FProtoDebugToggle Toggle;
        Toggle.Label = FText::FromString(TEXT("DebugDraw"));
        Toggle.GetValue = [DebugDrawCVar]() { return DebugDrawCVar->GetInt() != 0; };
        Toggle.SetValue = [DebugDrawCVar](bool bNewValue) { DebugDrawCVar->Set(bNewValue ? 1 : 0); };
        TuningSection.Toggles.Add(Toggle);
    }

    Sections.Add(TuningSection);

    DebugPanelWidget = SNew(SProtoDebugPanel).Sections(Sections);

    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->AddViewportWidgetContent(DebugPanelWidget.ToSharedRef());
    }

    FInputModeGameAndUI InputMode;
    InputMode.SetWidgetToFocus(DebugPanelWidget);
    InputMode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(InputMode);
    PC->SetShowMouseCursor(true);
#endif
}

void AProtoCharacter::Die()
{
    if (bIsDead)
    {
        return;
    }

    // 체력을 0으로 만들면 SetHealth가 OnPlayerDied를 브로드캐스트 -> HandleDeath(래그돌 + 소실) +
    // ARaidManager::HandlePlayerDied(사망 화면 + 허브 복귀)가 실제 사망과 똑같이 돈다.
    if (StatusComponent)
    {
        StatusComponent->SetHealth(0.0f);
    }
    else
    {
        HandleDeath();
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(93010, 2.0f, FColor::Red, TEXT("Debug: Die"));
    }
}

void AProtoCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        const FRotator Rotation = Controller->GetControlRotation();
        const FRotator YawRotation(0, Rotation.Yaw, 0);
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        AddMovementInput(ForwardDirection, MovementVector.X);
        AddMovementInput(RightDirection, MovementVector.Y);
    }
}

void AProtoCharacter::Look(const FInputActionValue& Value)
{
    if (bIsInvetoryOpened)
    {
        return;
    }

    const FVector2D LookAxisVector = Value.Get<FVector2D>();

    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}

void AProtoCharacter::Sprint(const FInputActionValue& Value)
{
    if (Value.Get<bool>())
    {
        StartSprint();
    }
    else
    {
        StopSprint();
    }
}

void AProtoCharacter::StartSprint()
{
    if (bIsDead) return;
    if (StatusComponent && StatusComponent->GetStamina() <= 0.0f) return;
    // 목마름/배고픔이 임계치(기본 20) 이하면 달릴 수 없다.
    if (StatusComponent && !StatusComponent->CanSprint()) return;

    bIsSprint = true;
    GetCharacterMovement()->MaxWalkSpeed = SprintWalkSpeed;
}

void AProtoCharacter::StopSprint()
{
    bIsSprint = false;
    GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AProtoCharacter::UpdateStamina(float DeltaTime)
{
    if (!StatusComponent || bIsDead) return;

    // 달리는 중 목마름/배고픔이 임계치 이하로 떨어지면 즉시 중단한다.
    if (bIsSprint && !StatusComponent->CanSprint())
    {
        StopSprint();
    }

    const bool bIsMovingWhileSprint = bIsSprint && GetVelocity().SizeSquared2D() > 1.0f;

    if (bIsMovingWhileSprint)
    {
        StatusComponent->SetStamina(StatusComponent->GetStamina() - StaminaDrainRate * DeltaTime);

        if (StatusComponent->GetStamina() <= 0.0f)
        {
            bStaminaDepleted = true;
            StaminaRegenTimer = StaminaRegenDelay;
            StopSprint();
        }
        return;
    }

    if (StatusComponent->GetStamina() >= StatusComponent->GetMaxStamina()) return;

    if (bStaminaDepleted)
    {
        StaminaRegenTimer -= DeltaTime;
        if (StaminaRegenTimer > 0.0f) return;
    }

    StatusComponent->SetStamina(StatusComponent->GetStamina() + StaminaRegenRate * DeltaTime);
    if (StatusComponent->GetStamina() >= StatusComponent->GetMaxStamina())
    {
        bStaminaDepleted = false;
    }
}

void AProtoCharacter::StartAim()
{
    bIsAiming = true;

    if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->TargetArmLength = AimCameraArmLength;
    }

    if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
    {
        Camera->SetRelativeLocation(AimCameraRelativeLocation);
    }

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AProtoCharacter::StopAim()
{
    bIsAiming = false;

    if (USpringArmComponent* SpringArm = FindComponentByClass<USpringArmComponent>())
    {
        SpringArm->TargetArmLength = DefaultCameraArmLength;
    }

    if (UCameraComponent* Camera = FindComponentByClass<UCameraComponent>())
    {
        Camera->SetRelativeLocation(DefaultCameraRelativeLocation);
    }

    bUseControllerRotationYaw = true;
    bUseControllerRotationPitch = false;
    bUseControllerRotationRoll = false;
    GetCharacterMovement()->bOrientRotationToMovement = false;
}

void AProtoCharacter::Interact(const FInputActionValue& Value)
{
    if (bIsContainerOpened)
    {
        CloseContainerScreen();
        return;
    }

    if (bIsLevelChangeOpened)
    {
        CloseLevelChangeScreen();
        return;
    }

    if (NearbyInteractables.IsEmpty()) return;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) return;

    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    const FVector TraceStart = CamLoc + CamRot.Vector() * 400.f;
    const FVector TraceEnd   = CamLoc + CamRot.Vector() * 700.f;
    GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
    // Demo build: interaction trace debug line disabled.

    AActor* HitActor = Hit.GetActor();
    if (!IsValid(HitActor) || !NearbyInteractables.Contains(HitActor)) return;

    if (HitActor->Implements<UInteractable>() && IInteractable::Execute_CanInteract(HitActor, this))
    {
        IInteractable::Execute_OnInteract(HitActor, this);
    }
}

ACompanionNPC* AProtoCharacter::GetCompanionNPC()
{
    return CachedCompanionNPC;
}

void AProtoCharacter::SpawnCompanion()
{
    if (!CompanionClass || !GetWorld())
    {
        return;
    }

    const FVector SpawnLocation = GetActorLocation() - GetActorForwardVector() * CompanionSpawnDistance;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    CachedCompanionNPC = GetWorld()->SpawnActor<ACompanionNPC>(CompanionClass, SpawnLocation, GetActorRotation(), SpawnParams);
    if (CachedCompanionNPC)
    {
        CachedCompanionNPC->SetOwningPlayer(this);

        // Hand the already-safely-loaded BP_CompanionNPC class to the net
        // subsystem so OTHER clients' companions can be shown as real
        // ACompanionNPC puppets instead of a placeholder -- see
        // UProtoNetClientSubsystem::SetRemoteCompanionClass for why this
        // can't just FClassFinder-load it there directly.
        if (UGameInstance* GameInstance = GetWorld()->GetGameInstance())
        {
            if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
            {
                NetClient->SetRemoteCompanionClass(CompanionClass);
            }
        }
    }
}

void AProtoCharacter::TalkToCompanionPressed(const FInputActionValue& Value)
{
    if (ACompanionNPC* Companion = GetCompanionNPC())
    {
        Companion->ListenComponent->StartListening();
    }
}

void AProtoCharacter::TalkToCompanionReleased(const FInputActionValue& Value)
{
    if (ACompanionNPC* Companion = GetCompanionNPC())
    {
        Companion->ListenComponent->StopListeningAndSend();
    }
}

void AProtoCharacter::PlayPickupAnimationIfUnarmed()
{
    if (CurrentWeaponType != EWeaponType::None || !PickupAnimation || !GetMesh())
    {
        return;
    }

    if (UAnimInstance* AnimInstance = GetMesh()->GetAnimInstance())
    {
        AnimInstance->PlaySlotAnimationAsDynamicMontage(PickupAnimation, TEXT("DefaultSlot"));
    }
}

void AProtoCharacter::OnInteractableEnter(AActor* Actor)
{
    if (!Actor) return;
    NearbyInteractables.AddUnique(Actor);
    if (DefaultUI && Actor->Implements<UInteractable>())
    {
        DefaultUI->AddInteractPrompt(Actor, IInteractable::Execute_GetInteractPrompt(Actor));
    }
}

void AProtoCharacter::OnInteractableExit(AActor* Actor)
{
    if (!Actor) return;
    NearbyInteractables.Remove(Actor);
    if (DefaultUI) DefaultUI->RemoveInteractPrompt(Actor);

    if (bIsContainerOpened) CloseContainerScreen();
    if (bIsLevelChangeOpened) CloseLevelChangeScreen();
}

void AProtoCharacter::OpenContainerScreen(AItemContainerBase* Container)
{
    if (!ContainerWidgetClass) return;

    if (ContainerWidgetInstance == nullptr)
    {
        ContainerWidgetInstance = CreateWidget<UContainerScreenWidget>(GetWorld(), ContainerWidgetClass);
    }

    if (!ContainerWidgetInstance) return;

    ContainerWidgetInstance->InitializeScreen(InventoryComponent, Container->ContainerInventory);
    ContainerWidgetInstance->AddToViewport();
    bIsContainerOpened = true;
    bIsInvetoryOpened = true;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(ContainerWidgetInstance->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
    }
}

void AProtoCharacter::CloseContainerScreen()
{
    if (ContainerWidgetInstance) ContainerWidgetInstance->RemoveFromParent();
    bIsContainerOpened = false;
    bIsInvetoryOpened = false;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AProtoCharacter::OpenLevelChangeScreen(ALevelChanger* Changer)
{
    if (!LevelChangeWidgetClass) return;

    if (LevelChangeWidgetInstance == nullptr)
    {
        LevelChangeWidgetInstance = CreateWidget<ULevelChangeSelectWidget>(GetWorld(), LevelChangeWidgetClass);
    }

    if (!LevelChangeWidgetInstance) return;

    LevelChangeWidgetInstance->AddToViewport();
    bIsLevelChangeOpened = true;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetWidgetToFocus(LevelChangeWidgetInstance->TakeWidget());
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
    }
}

void AProtoCharacter::CloseLevelChangeScreen()
{
    if (LevelChangeWidgetInstance) LevelChangeWidgetInstance->RemoveFromParent();
    bIsLevelChangeOpened = false;

    APlayerController* PC = Cast<APlayerController>(Controller);
    if (PC)
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AProtoCharacter::ToggleInventory(const FInputActionValue& Value)
{
    if (bIsContainerOpened) return;

    if (InventoryWidgetInstance == nullptr && InventoryWidgetClass != nullptr)
    {
        InventoryWidgetInstance = CreateWidget<UUserWidget>(GetWorld(), InventoryWidgetClass);
    }

    if (InventoryWidgetInstance != nullptr)
    {
        APlayerController* PlayerController = Cast<APlayerController>(Controller);
        if (InventoryWidgetInstance->IsInViewport())
        {
            bIsInvetoryOpened = false;
            InventoryWidgetInstance->RemoveFromParent();
            if (PlayerController)
            {
                PlayerController->SetShowMouseCursor(false);
                FInputModeGameOnly InputMode;
                PlayerController->SetInputMode(InputMode);
            }
        }
        else
        {
            InventoryWidgetInstance->AddToViewport();
            bIsInvetoryOpened = true;
            if (UInventoryScreenWidget* InvUI = Cast<UInventoryScreenWidget>(InventoryWidgetInstance))
            {
                InvUI->InitializeGrid(InventoryComponent);
                InvUI->InitializeEquipment(EquipmentComponent);
                InvUI->InitializeQuickSlots(QuickSlotComponent);
            }
            if (PlayerController)
            {
                PlayerController->SetShowMouseCursor(true);
                FInputModeGameAndUI InputMode;
                InputMode.SetWidgetToFocus(InventoryWidgetInstance->TakeWidget());
                InputMode.SetHideCursorDuringCapture(false);
                PlayerController->SetInputMode(InputMode);
                InventoryWidgetInstance->SetUserFocus(PlayerController);
            }
        }
    }
}

void AProtoCharacter::SetWeaponTypeNone()
{
    if (Swapping > 0.0f || CurrentWeaponType == EWeaponType::None)
    {
        return;
    }

    CurrentWeapon = GetWeaponByType(CurrentWeaponType);
    BeginWeaponSwap(EWeaponType::None);
}

void AProtoCharacter::SetWeaponSlot1()
{
    SetWeaponFromSlot(EEquipmentSlot::Weapon1);
}

void AProtoCharacter::SetWeaponSlot2()
{
    SetWeaponFromSlot(EEquipmentSlot::Weapon2);
}

void AProtoCharacter::SetWeaponFromSlot(EEquipmentSlot Slot)
{
    if (Swapping > 0.0f)
    {
        return;
    }

    AWeaponBase* const* Found = EquippedWeaponActors.Find(Slot);
    AWeaponBase* SlotWeapon = Found ? *Found : nullptr;
    if (!SlotWeapon)
    {
        if (GEngine)
        {
        }
        return;
    }

    if (CurrentWeaponType != EWeaponType::None && SlotWeapon->WeaponType != EWeaponType::None && CurrentWeaponType != SlotWeapon->WeaponType)
    {
        BeginWeaponToWeaponSwap(SlotWeapon->WeaponType, SlotWeapon);
        return;
    }

    CurrentWeapon = SlotWeapon;
    BeginWeaponSwap(SlotWeapon->WeaponType, SlotWeapon);
}

AWeaponBase* AProtoCharacter::GetWeaponByType(EWeaponType WeaponType) const
{
    switch (WeaponType)
    {
    case EWeaponType::Rifle:
        return CurrentRifle;
    case EWeaponType::Pistol:
        return CurrentPistol;
    default:
        return nullptr;
    }
}

AWeaponBase* AProtoCharacter::SpawnFallbackRemoteWeapon(EWeaponType WeaponType)
{
    if (WeaponType != EWeaponType::Rifle && WeaponType != EWeaponType::Pistol)
    {
        return nullptr;
    }
    if (!GetMesh())
    {
        return nullptr;
    }

    const TSubclassOf<AWeaponBase> WeaponClass = WeaponType == EWeaponType::Rifle ? RemoteRifleClass : RemotePistolClass;
    if (!WeaponClass)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    AWeaponBase* WeaponActor = GetWorld()->SpawnActor<AWeaponBase>(WeaponClass, GetActorTransform(), SpawnParams);
    if (!WeaponActor)
    {
        return nullptr;
    }

    WeaponActor->SetOwner(this);
    WeaponActor->SetInstigator(this);
    WeaponActor->SetActorEnableCollision(false);

    if (UStaticMeshComponent* WeaponMesh = WeaponActor->WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetGenerateOverlapEvents(false);
    }
    if (UBoxComponent* CollisionBox = WeaponActor->CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionBox->SetGenerateOverlapEvents(false);
    }

    // Start holstered; the caller (ApplyRemoteWeaponEquip / HandleProgressRestored)
    // is the one deciding whether it should actually be drawn.
    const FName StorageSocketName = WeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage");
    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);
    WeaponActor->AttachToComponent(GetMesh(), AttachRules, StorageSocketName);

    if (WeaponType == EWeaponType::Rifle)
    {
        CurrentRifle = WeaponActor;
    }
    else
    {
        CurrentPistol = WeaponActor;
    }

    return WeaponActor;
}

void AProtoCharacter::BeginWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor)
{
    StopFireWeapon();

    if (Swapping > 0.0f)
    {
        return;
    }

    if (CurrentWeaponType == TargetWeaponType)
    {
        return;
    }

    AWeaponBase* SwapWeapon = TargetWeaponType == EWeaponType::None
        ? CurrentWeapon
        : (TargetWeaponActor ? TargetWeaponActor : GetWeaponByType(TargetWeaponType));

    if (!SwapWeapon)
    {
        if (GEngine)
        {
        }
        return;
    }

    StopAim();

    CurrentWeapon = SwapWeapon;

    if (TargetWeaponType != EWeaponType::None)
    {
        Joint = CurrentWeapon->LeftHandJointTarget;

        if (GEngine)
        {
        }
    }

    const EWeaponType PreviousWeaponType = CurrentWeaponType;
    SwapFromWeaponType = PreviousWeaponType;

    PendingWeaponType = TargetWeaponType;
    CurrentWeaponType = TargetWeaponType;
    bHasWeapon = CurrentWeaponType != EWeaponType::None;

    /*-------------------
     네트워킹: 무기 장착 브로드캐스트
    -------------------*/
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendWeaponEquip(static_cast<uint8>(TargetWeaponType));
        }
    }

    const bool bIsEquippingWeapon = TargetWeaponType != EWeaponType::None;
    Swapping = bIsEquippingWeapon ? SwapWeapon->EquipSwapTime : SwapWeapon->UnequipSwapTime;
    Swapping = FMath::Max(0.0f, Swapping);
    SwappingAlpha = false;

    if (WeaponSwapMontage)
    {
        if (TargetWeaponType == EWeaponType::None)
        {
            if (PreviousWeaponType == EWeaponType::Rifle)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, RifleToHandSectionName);
            }
            else if (PreviousWeaponType == EWeaponType::Pistol)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, PistolToHandSectionName);
            }
        }
        else if (PreviousWeaponType == EWeaponType::None && TargetWeaponType == EWeaponType::Rifle)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToRifleSectionName);
        }
        else if (PreviousWeaponType == EWeaponType::None && TargetWeaponType == EWeaponType::Pistol)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToPistolSectionName);
        }
    }
    if (Swapping <= 0.0f)
    {
        FinishWeaponSwap();
        return;
    }

    if (GEngine)
    {
    }
}

void AProtoCharacter::BeginWeaponToWeaponSwap(EWeaponType TargetWeaponType, AWeaponBase* TargetWeaponActor)
{
    StopFireWeapon();

    if (Swapping > 0.0f || CurrentWeaponType == EWeaponType::None || TargetWeaponType == EWeaponType::None)
    {
        return;
    }

    if (CurrentWeaponType == TargetWeaponType)
    {
        return;
    }

    AWeaponBase* NextWeapon = TargetWeaponActor ? TargetWeaponActor : GetWeaponByType(TargetWeaponType);
    if (!NextWeapon || !GetMesh())
    {
        return;
    }

    const EWeaponType PreviousWeaponType = CurrentWeaponType;
    AWeaponBase* PreviousWeapon = GetWeaponByType(PreviousWeaponType);
    if (!PreviousWeapon)
    {
        PreviousWeapon = CurrentWeapon;
    }

    StopAim();

    if (PreviousWeapon)
    {
        const FName PreviousStorageSocketName = PreviousWeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage");
        const FAttachmentTransformRules StorageAttachRules(
            EAttachmentRule::SnapToTarget,
            EAttachmentRule::SnapToTarget,
            EAttachmentRule::KeepRelative,
            true);
        PreviousWeapon->AttachToComponent(GetMesh(), StorageAttachRules, PreviousStorageSocketName);
        PreviousWeapon->SetActorEnableCollision(false);
    }

    CurrentWeapon = NextWeapon;
    Joint = CurrentWeapon->LeftHandJointTarget;
    SwapFromWeaponType = PreviousWeaponType;
    PendingWeaponType = TargetWeaponType;
    CurrentWeaponType = TargetWeaponType;
    bHasWeapon = true;

    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendWeaponEquip(static_cast<uint8>(TargetWeaponType));
        }
    }

    Swapping = FMath::Max(0.0f, CurrentWeapon->EquipSwapTime);
    SwappingAlpha = false;

    if (WeaponSwapMontage)
    {
        if (TargetWeaponType == EWeaponType::Rifle)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToRifleSectionName);
        }
        else if (TargetWeaponType == EWeaponType::Pistol)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToPistolSectionName);
        }
    }

    if (Swapping <= 0.0f)
    {
        FinishWeaponSwap();
    }
}
void AProtoCharacter::FinishWeaponSwap()
{
    Swapping = 0.0f;
    SwappingAlpha = true;
    CurrentWeaponType = PendingWeaponType;

    if (SwapFromWeaponType != EWeaponType::None && SwapFromWeaponType != CurrentWeaponType)
    {
        if (AWeaponBase* PreviousWeapon = GetWeaponByType(SwapFromWeaponType))
        {
            const FName PreviousStorageSocketName = SwapFromWeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage");
            const FAttachmentTransformRules StorageAttachRules(
                EAttachmentRule::SnapToTarget,
                EAttachmentRule::SnapToTarget,
                EAttachmentRule::KeepRelative,
                true);
            PreviousWeapon->AttachToComponent(GetMesh(), StorageAttachRules, PreviousStorageSocketName);
            PreviousWeapon->SetActorEnableCollision(false);
        }
    }

    if (CurrentWeapon)
    {
        Joint = CurrentWeapon->LeftHandJointTarget;
    }

    if (CurrentWeaponType == EWeaponType::None)
    {
        bHasWeapon = false; 
        AttachCurrentWeaponToSocket(SwapFromWeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage"));

        if (GEngine)
        {
        }
        return;
    }

    if (CurrentWeaponType == EWeaponType::Rifle)
    {
        bHasWeapon = true;
        AttachCurrentWeaponToSocket(TEXT("WeaponSocket"));

        if (GEngine)
        {
        }
        return;
    }

    if (CurrentWeaponType == EWeaponType::Pistol)
    {
        bHasWeapon = true;
        AttachCurrentWeaponToSocket(TEXT("PistolSocket"));

        if (GEngine)
        {
        }
    }
}
void AProtoCharacter::StartFireWeapon()
{
    if (!CurrentWeapon || CurrentWeaponType == EWeaponType::None || !bIsAiming || Swapping > 0.0f || bIsReloading || !CurrentWeapon->CanFire())
    {
        return;
    }

    FireWeapon();

    if (!CurrentWeapon->bAutomatic || CurrentWeapon->FireRate <= 0.0f)
    {
        return;
    }

    const float FireInterval = 1.0f / CurrentWeapon->FireRate;
    GetWorldTimerManager().SetTimer(AutoFireTimerHandle, this, &AProtoCharacter::FireWeapon, FireInterval, true, FireInterval);
}

void AProtoCharacter::StopFireWeapon()
{
    GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
}

void AProtoCharacter::FireWeapon()
{
    if (!CurrentWeapon || CurrentWeaponType == EWeaponType::None || !bIsAiming || Swapping > 0.0f || bIsReloading || !CurrentWeapon->CanFire())
    {
        return;
    }

    CurrentWeapon->Fire();
    ApplyWeaponRecoil();
}

void AProtoCharacter::ApplyWeaponRecoil()
{
    if (!IsLocallyControlled() || !Controller)
    {
        return;
    }

    float PitchKick = RifleRecoilPitch;
    float YawKick = FMath::RandRange(-RifleRecoilYaw, RifleRecoilYaw);
    TSubclassOf<UCameraShakeBase> ShakeClass = RifleFireCameraShakeClass;

    if (CurrentWeaponType == EWeaponType::Pistol)
    {
        PitchKick = PistolRecoilPitch;
        YawKick = FMath::RandRange(-PistolRecoilYaw, PistolRecoilYaw);
        ShakeClass = PistolFireCameraShakeClass;
    }

    AddControllerPitchInput(-PitchKick);
    AddControllerYawInput(YawKick);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (PC->PlayerCameraManager && ShakeClass)
        {
            PC->PlayerCameraManager->StartCameraShake(ShakeClass);
        }
    }
}

void AProtoCharacter::HandleMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    static const FName AmmoAttachNotifyName(TEXT("AmmoAttach"));
    static const FName AmmoDetachNotifyName(TEXT("AmmoDetach"));
    static const FName NewAmmoNotifyName(TEXT("NewAmmo"));
    static const FName NewAmmoAttachNotifyName(TEXT("NewAmmoAttach"));
    static const FName NewAmmoDetachNotifyName(TEXT("NewAmmoDetach"));

    if (NotifyName == AmmoAttachNotifyName)
    {
        ReloadAmmoAttach();
    }
    else if (NotifyName == AmmoDetachNotifyName)
    {
        ReloadAmmoDetach();
    }
    else if (NotifyName == NewAmmoNotifyName)
    {
        ReloadNewAmmo();
    }
    else if (NotifyName == NewAmmoAttachNotifyName)
    {
        if (CurrentWeaponType == EWeaponType::Pistol)
        {
            ReloadNewAmmo();
        }
        else
        {
            ReloadNewAmmoAttach();
        }
    }
    else if (NotifyName == NewAmmoDetachNotifyName)
    {
        ReloadNewAmmoAttach();
    }
    else
    {
        return;
    }
}
void AProtoCharacter::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (Montage != RifleReloadMontage)
    {
        return;
    }

    if (bIsReloading)
    {
        if (!bInterrupted && CurrentWeapon)
        {
            CurrentWeapon->ReloadMagazine();
        }

        bIsReloading = false;
        SwappingAlpha = CurrentWeapon && CurrentWeaponType != EWeaponType::None && Swapping <= 0.0f;
    }

    if (bIsUsingConsumable)
    {
        EndConsumableAnimationState();
    }
}
void AProtoCharacter::ReloadWeapon()
{
    if (Swapping > 0.0f || !CurrentWeapon || !CurrentWeapon->CanReload())
    {
        return;
    }

    const bool bCanReload = CurrentWeaponType == EWeaponType::Rifle || CurrentWeaponType == EWeaponType::Pistol;
    if (!bCanReload)
    {
        return;
    }

    StopAim();

    StopFireWeapon();

    if (!RifleReloadMontage)
    {
        if (GEngine)
        {
        }
        return;
    }

    bIsReloading = true;
    SwappingAlpha = false;

    const FName ReloadSectionName = CurrentWeaponType == EWeaponType::Pistol ? PistolReloadSectionName : RifleReloadSectionName;
    const float MontageLength = PlayAnimMontage(RifleReloadMontage, 1.0f, ReloadSectionName);
    if (MontageLength > 0.0f)
    {
        CurrentWeapon->PlayReloadSound();
    }
    else
    {
        bIsReloading = false;
        SwappingAlpha = true;
    }

    /*-------------------
     네트워킹: 재장전 브로드캐스트
    -------------------*/
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendWeaponReload(static_cast<uint8>(CurrentWeaponType));
        }
    }
}

/*-------------------
 네트워킹: 원격 플레이어 시각 동기화
-------------------*/
void AProtoCharacter::PlayRemoteReloadMontage(EWeaponType ForWeaponType)
{
    if (!RifleReloadMontage)
    {
        return;
    }

    const FName ReloadSectionName = ForWeaponType == EWeaponType::Pistol ? PistolReloadSectionName : RifleReloadSectionName;
    if (PlayAnimMontage(RifleReloadMontage, 1.0f, ReloadSectionName) > 0.0f)
    {
        if (AWeaponBase* ReloadWeaponActor = GetWeaponByType(ForWeaponType))
        {
            ReloadWeaponActor->PlayReloadSound();
        }
    }
}

void AProtoCharacter::ApplyRemoteWeaponEquip(EWeaponType ForWeaponType)
{
    if (Swapping > 0.0f || ForWeaponType == CurrentWeaponType)
    {
        return;
    }

    const EWeaponType PreviousWeaponType = CurrentWeaponType;
    // Storing (ForWeaponType == None) needs the weapon that's about to be
    // holstered, not a lookup by the (already-None) target type.
    AWeaponBase* SwapWeapon = ForWeaponType == EWeaponType::None ? GetWeaponByType(PreviousWeaponType) : GetWeaponByType(ForWeaponType);
    if (!SwapWeapon && ForWeaponType != EWeaponType::None)
    {
        // Remote character (see SpawnFallbackRemoteWeapon's header comment)
        // being equipped with a weapon type it has never shown before --
        // spawn one instead of just tracking the logical state, or the
        // sender's weapon would never actually be visible to us.
        SwapWeapon = SpawnFallbackRemoteWeapon(ForWeaponType);
    }
    if (!SwapWeapon)
    {
        // Storing with nothing equipped (or the fallback spawn failed): just
        // snap the logical state so later broadcasts stay consistent.
        CurrentWeaponType = ForWeaponType;
        bHasWeapon = ForWeaponType != EWeaponType::None;
        return;
    }

    CurrentWeapon = SwapWeapon;
    SwapFromWeaponType = PreviousWeaponType;
    PendingWeaponType = ForWeaponType;
    CurrentWeaponType = ForWeaponType;
    bHasWeapon = CurrentWeaponType != EWeaponType::None;

    // Same swap-timer/montage flow as BeginWeaponSwap(), minus the
    // SendWeaponEquip() broadcast (this transition came FROM the network;
    // echoing it back would loop). Tick()'s Swapping countdown isn't gated
    // to the local player, so it drives FinishWeaponSwap() for us -- that's
    // also what does the actual socket attach, timed to match the montage.
    const bool bIsEquippingWeapon = ForWeaponType != EWeaponType::None;
    Swapping = bIsEquippingWeapon ? SwapWeapon->EquipSwapTime : SwapWeapon->UnequipSwapTime;
    Swapping = FMath::Max(0.0f, Swapping);
    SwappingAlpha = false;

    if (WeaponSwapMontage)
    {
        if (ForWeaponType == EWeaponType::None)
        {
            if (PreviousWeaponType == EWeaponType::Rifle)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, RifleToHandSectionName);
            }
            else if (PreviousWeaponType == EWeaponType::Pistol)
            {
                PlayAnimMontage(WeaponSwapMontage, 1.0f, PistolToHandSectionName);
            }
        }
        else if (PreviousWeaponType == EWeaponType::None && ForWeaponType == EWeaponType::Rifle)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToRifleSectionName);
        }
        else if (PreviousWeaponType == EWeaponType::None && ForWeaponType == EWeaponType::Pistol)
        {
            PlayAnimMontage(RifleReloadMontage ? RifleReloadMontage : WeaponSwapMontage, 1.0f, HandToPistolSectionName);
        }
    }

    if (Swapping <= 0.0f)
    {
        FinishWeaponSwap();
    }
}

void AProtoCharacter::SetRemoteAiming(bool bAiming, float Pitch)
{
    bIsAiming = bAiming;
    // Same clamp Tick() uses for the locally-controlled player's own
    // AimPitch (see the Controller-gated block near the top of Tick()).
    AimPitch = FMath::Clamp(FRotator::NormalizeAxis(Pitch), -30.0f, 30.0f);
}

void AProtoCharacter::HandleProgressRestored(FVector Position, FRotator Look, uint8 WeaponType)
{
    // This can run two ways: BeginPlay's direct ConsumePendingProgressRestore()
    // pull (which already cleared the pending cache), or the live
    // OnProgressRestored broadcast catching a character that already existed
    // when S2C_LoginSuccess arrived (old in-game Slate popup reconnect --
    // that character's BeginPlay ran before the login, so nothing was
    // pending to pull). In the second case the pending cache is still set
    // and would otherwise sit there and get wrongly replayed onto whatever
    // unrelated character spawns next (e.g. walking into a LevelChanger to
    // the Single/Multi map) -- clear it here too so a restore is only ever
    // applied once, to whichever character actually received it.
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            FVector UnusedPos; FRotator UnusedLook; uint8 UnusedWeapon;
            NetClient->ConsumePendingProgressRestore(UnusedPos, UnusedLook, UnusedWeapon);
        }
    }

    SetActorLocation(Position);
    if (Controller)
    {
        Controller->SetControlRotation(Look);
    }

    const EWeaponType RestoredType = static_cast<EWeaponType>(WeaponType);
    if (RestoredType == EWeaponType::None)
    {
        return;
    }

    AWeaponBase* RestoredWeapon = GetWeaponByType(RestoredType);
    if (!RestoredWeapon)
    {
        RestoredWeapon = SpawnFallbackRemoteWeapon(RestoredType);
    }
    if (!RestoredWeapon)
    {
        // Fallback spawn also failed (e.g. no RemoteRifleClass/RemotePistolClass
        // set) -- the weapon_type is still tracked server-side, so a
        // subsequent equip/store still broadcasts correctly even though the
        // visual didn't restore here.
        return;
    }

    CurrentWeapon = RestoredWeapon;
    Joint = CurrentWeapon->LeftHandJointTarget;
    CurrentWeaponType = RestoredType;
    PendingWeaponType = RestoredType;
    bHasWeapon = true;

    AttachCurrentWeaponToSocket(RestoredType == EWeaponType::Pistol ? TEXT("PistolSocket") : TEXT("WeaponSocket"));
}

UItemDataBase* AProtoCharacter::ResolveItemDataByName(const FString& AssetName) const
{
    static TMap<FString, TWeakObjectPtr<UItemDataBase>> Cache;

    if (const TWeakObjectPtr<UItemDataBase>* Cached = Cache.Find(AssetName))
    {
        if (Cached->IsValid())
        {
            return Cached->Get();
        }
        Cache.Remove(AssetName);
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    // The background discovery scan isn't guaranteed to have finished by
    // the time this runs -- this can fire from BeginPlay right after a
    // fresh app launch or a level travel, both of which can land well
    // before a content-heavy project's initial scan completes. If it's
    // still running, GetAssetsByClass below would silently return
    // whatever's been discovered SO FAR, which can be missing entries and
    // fail every single lookup with no error (just "couldn't resolve item
    // asset" warnings from the caller) -- force it to finish first rather
    // than risk that. No-op (returns immediately) once the scan is done,
    // which is the overwhelmingly common case, so this costs nothing then.
    if (AssetRegistryModule.Get().IsLoadingAssets())
    {
        AssetRegistryModule.Get().SearchAllAssets(/*bSynchronousSearch=*/true);
    }

    TArray<FAssetData> AssetDataList;
    AssetRegistryModule.Get().GetAssetsByClass(UItemDataBase::StaticClass()->GetClassPathName(), AssetDataList, /*bSearchSubClasses=*/true);

    for (const FAssetData& AssetData : AssetDataList)
    {
        if (AssetData.AssetName.ToString() != AssetName)
        {
            continue;
        }

        if (UItemDataBase* Resolved = Cast<UItemDataBase>(AssetData.GetAsset()))
        {
            Cache.Add(AssetName, Resolved);
            return Resolved;
        }
    }

    return nullptr;
}

void AProtoCharacter::HandleInventoryRestored(const TArray<FProtoInventoryItemEntry>& Items)
{
    // See the matching comment in HandleProgressRestored -- same reasoning,
    // same fix: make sure the pending cache can't outlive this delivery and
    // get replayed onto a later, unrelated character.
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            TArray<FProtoInventoryItemEntry> UnusedItems;
            TArray<FProtoEquipmentEntry> UnusedEquipment;
            TArray<FProtoQuickSlotEntry> UnusedQuickSlots;
            NetClient->ConsumePendingInventoryRestore(UnusedItems, UnusedEquipment, UnusedQuickSlots);
        }
    }

    if (!InventoryComponent)
    {
        return;
    }

    bIsRestoringInventory = true;

    // The restored list is this account's FULL saved grid, not an addition
    // to whatever's already here -- without this, whatever the character
    // spawned with (another account's leftover items, if this same running
    // client logged out and into a different account; or default starting
    // items) would still be sitting in the grid, and the very next
    // OnInventoryChanged save would write that mixed-together state back
    // out under THIS account. This is what "인벤토리가 계정별로 안 됨" was:
    // restore only ever added, never replaced.
    InventoryComponent->Items.Empty();

    for (const FProtoInventoryItemEntry& Entry : Items)
    {
        UItemDataBase* ItemData = ResolveItemDataByName(Entry.ItemId.ToString());
        if (!ItemData)
        {
            UE_LOG(LogTemp, Warning, TEXT("HandleInventoryRestored: couldn't resolve item asset '%s', skipping"), *Entry.ItemId.ToString());
            continue;
        }

        InventoryComponent->AddItemAt(ItemData, FIntPoint(Entry.GridX, Entry.GridY), Entry.bRotated, Entry.StackCount);
    }

    bIsRestoringInventory = false;
}

void AProtoCharacter::HandleInventoryChanged()
{
    // bIsRestoringEquipment too: RestoreEquipmentAndQuickSlots's temporary
    // grid-then-equip/register dance (see its own comment) fires this via
    // AddItemAt/RemoveInstanceById several times for state that's about to
    // move again anyway -- HandleInventoryChanged will get a real, settled
    // trigger once actual gameplay changes the grid.
    if (bIsRestoringInventory || bIsRestoringEquipment || !InventoryComponent)
    {
        return;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
    if (!NetClient)
    {
        return;
    }

    NetClient->SendSaveInventory(BuildInventorySnapshot());
}

void AProtoCharacter::HandleEquipmentChangedForSave(EEquipmentSlot ChangedSlot)
{
    if (bIsRestoringEquipment || !EquipmentComponent)
    {
        return;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
    if (!NetClient)
    {
        return;
    }

    NetClient->SendSaveEquipment(BuildEquipmentSnapshot());
}

void AProtoCharacter::HandleQuickSlotChangedForSave(int32 SlotIndex)
{
    if (bIsRestoringEquipment || !QuickSlotComponent)
    {
        return;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
    if (!NetClient)
    {
        return;
    }

    NetClient->SendSaveQuickSlots(BuildQuickSlotSnapshot());
}

void AProtoCharacter::HandleEnemyAttackPlayer(int32 EnemyId, float Damage)
{
    if (bIsDead) return;

    if (StatusComponent)
    {
        StatusComponent->SetHealth(StatusComponent->GetHealth() - Damage);
    }
}

void AProtoCharacter::HandleDeath()
{
    if (bIsDead) return;
    bIsDead = true;

    // 사격 중단.
    StopFireWeapon();
    GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);

    // 입력 차단(컨트롤러는 유지 - ARaidManager가 레벨 이동을 처리한다).
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        DisableInput(PC);
    }

    ApplyRagdollVisual();

    // 지니고 있던 것 전부 소실(스태시는 별개).
    WipeCarriedInventoryOnDeath();

    // Other clients' mirror of this player should ragdoll too instead of
    // standing there frozen -- see S2C_PlayerDied's schema comment.
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendPlayerDied();
        }
    }
}

void AProtoCharacter::HandleRemotePlayerDied()
{
    // Deliberately does NOT reuse HandleDeath(): that function also stops
    // this client's own auto-fire timer, disables ITS OWN player's input,
    // and wipes+re-saves an inventory -- all local-player-only state a
    // remote mirror doesn't meaningfully have. Calling it directly on a
    // remote instance would, at best, no-op those parts; at worst (the
    // inventory wipe triggers OnInventoryChanged -> HandleInventoryChanged
    // -> SendSaveInventory) it could push this remote instance's empty
    // inventory to the server under THIS client's own logged-in account,
    // silently corrupting their real saved inventory. Ragdoll only.
    if (bIsDead) return;
    bIsDead = true;

    ApplyRagdollVisual();
}

void AProtoCharacter::ApplyRagdollVisual()
{
    // 이동 정지.
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->DisableMovement();
        Movement->StopMovementImmediately();
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        CharacterMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
        CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
        CharacterMesh->SetCollisionObjectType(ECC_PhysicsBody);
        CharacterMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        CharacterMesh->SetAllBodiesBelowSimulatePhysics(NAME_None, true, true);
        CharacterMesh->SetAllBodiesSimulatePhysics(true);
        CharacterMesh->SetSimulatePhysics(true);
        CharacterMesh->WakeAllRigidBodies();
        CharacterMesh->bBlendPhysics = true;

#if !(UE_BUILD_SHIPPING)
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(
                93011,
                4.0f,
                CharacterMesh->IsSimulatingPhysics() ? FColor::Green : FColor::Red,
                FString::Printf(TEXT("Player Ragdoll: PhysicsAsset=%s Sim=%s Collision=%d"),
                    CharacterMesh->GetPhysicsAsset() ? TEXT("YES") : TEXT("NO"),
                    CharacterMesh->IsSimulatingPhysics() ? TEXT("YES") : TEXT("NO"),
                    static_cast<int32>(CharacterMesh->GetCollisionEnabled())));
        }
#endif
    }
}

void AProtoCharacter::WipeCarriedInventoryOnDeath()
{
    if (EquipmentComponent)
    {
        EquipmentComponent->ClearAll();
    }
    if (QuickSlotComponent)
    {
        QuickSlotComponent->ClearAll();
    }
    if (InventoryComponent)
    {
        InventoryComponent->Items.Empty();
        // OnInventoryChanged → HandleInventoryChanged가 빈 인벤토리를 서버에 저장하고 UI를 갱신한다.
        InventoryComponent->OnInventoryChanged.Broadcast();
    }
}

TArray<FProtoInventoryItemEntry> AProtoCharacter::BuildInventorySnapshot() const
{
    TArray<FProtoInventoryItemEntry> Snapshot;
    if (!InventoryComponent)
    {
        return Snapshot;
    }

    Snapshot.Reserve(InventoryComponent->Items.Num());
    for (const FInventoryItemInstance& Item : InventoryComponent->Items)
    {
        if (!Item.ItemData)
        {
            continue;
        }

        FProtoInventoryItemEntry Entry;
        Entry.ItemId = FName(*Item.ItemData->GetName());
        Entry.GridX = Item.GridPosition.X;
        Entry.GridY = Item.GridPosition.Y;
        Entry.bRotated = Item.bIsRotated;
        Entry.StackCount = Item.StackCount;
        Snapshot.Add(Entry);
    }
    return Snapshot;
}

TArray<FProtoEquipmentEntry> AProtoCharacter::BuildEquipmentSnapshot() const
{
    TArray<FProtoEquipmentEntry> Snapshot;
    if (!EquipmentComponent)
    {
        return Snapshot;
    }

    static const EEquipmentSlot AllSlots[] = { EEquipmentSlot::Helmet, EEquipmentSlot::Vest, EEquipmentSlot::Weapon1, EEquipmentSlot::Weapon2 };
    for (EEquipmentSlot Slot : AllSlots)
    {
        const FEquippedItem& Equipped = EquipmentComponent->GetEquippedItem(Slot);
        if (!Equipped.ItemData)
        {
            continue;
        }

        FProtoEquipmentEntry Entry;
        Entry.ItemId = FName(*Equipped.ItemData->GetName());
        Entry.Slot = static_cast<int32>(Slot);
        Snapshot.Add(Entry);
    }
    return Snapshot;
}

TArray<FProtoQuickSlotEntry> AProtoCharacter::BuildQuickSlotSnapshot() const
{
    TArray<FProtoQuickSlotEntry> Snapshot;
    if (!QuickSlotComponent)
    {
        return Snapshot;
    }

    for (int32 SlotIndex = 0; SlotIndex < QuickSlotComponent->NumSlots; ++SlotIndex)
    {
        const FQuickSlotEntry& Slot = QuickSlotComponent->GetQuickSlotEntry(SlotIndex);
        if (!Slot.ItemData)
        {
            continue;
        }

        FProtoQuickSlotEntry Entry;
        Entry.ItemId = FName(*Slot.ItemData->GetName());
        Entry.SlotIndex = SlotIndex;
        Entry.StackCount = Slot.StackCount;
        Snapshot.Add(Entry);
    }
    return Snapshot;
}

void AProtoCharacter::RestoreEquipmentAndQuickSlots(const TArray<FProtoEquipmentEntry>& Equipment, const TArray<FProtoQuickSlotEntry>& QuickSlots)
{
    if (!InventoryComponent)
    {
        return;
    }

    // Suppresses HandleInventoryChanged/HandleEquipmentChangedForSave/
    // HandleQuickSlotChangedForSave for every intermediate step below (each
    // temporary grid insertion, each equip/register move) -- none of that
    // is a real, settled state worth a network round trip on its own. One
    // explicit save of each at the end (once this flag is back off)
    // persists the actual final result instead.
    bIsRestoringEquipment = true;

    for (const FProtoEquipmentEntry& Entry : Equipment)
    {
        if (!EquipmentComponent)
        {
            break;
        }

        UItemDataBase* ItemData = ResolveItemDataByName(Entry.ItemId.ToString());
        if (!ItemData)
        {
            UE_LOG(LogTemp, Warning, TEXT("RestoreEquipmentAndQuickSlots: couldn't resolve equipped item asset '%s', skipping"), *Entry.ItemId.ToString());
            continue;
        }

        FIntPoint TempPosition;
        if (!InventoryComponent->FindEmptySpace(FIntPoint(ItemData->GridWidth, ItemData->GridHeight), TempPosition)
            || !InventoryComponent->AddItemAt(ItemData, TempPosition, false, 1))
        {
            UE_LOG(LogTemp, Warning, TEXT("RestoreEquipmentAndQuickSlots: no room to restore equipped item '%s', dropping it"), *Entry.ItemId.ToString());
            continue;
        }

        EquipmentComponent->EquipFromInventory(InventoryComponent, InventoryComponent->Items.Last().InstanceId, static_cast<EEquipmentSlot>(Entry.Slot));
    }

    for (const FProtoQuickSlotEntry& Entry : QuickSlots)
    {
        if (!QuickSlotComponent)
        {
            break;
        }

        UItemDataBase* ItemData = ResolveItemDataByName(Entry.ItemId.ToString());
        if (!ItemData)
        {
            UE_LOG(LogTemp, Warning, TEXT("RestoreEquipmentAndQuickSlots: couldn't resolve quick-slot item asset '%s', skipping"), *Entry.ItemId.ToString());
            continue;
        }

        FIntPoint TempPosition;
        if (!InventoryComponent->FindEmptySpace(FIntPoint(ItemData->GridWidth, ItemData->GridHeight), TempPosition)
            || !InventoryComponent->AddItemAt(ItemData, TempPosition, false, Entry.StackCount))
        {
            UE_LOG(LogTemp, Warning, TEXT("RestoreEquipmentAndQuickSlots: no room to restore quick-slot item '%s', dropping it"), *Entry.ItemId.ToString());
            continue;
        }

        QuickSlotComponent->RegisterFromInventory(Entry.SlotIndex, InventoryComponent, InventoryComponent->Items.Last().InstanceId);
    }

    bIsRestoringEquipment = false;

    // Persist the actual settled result once, now that the suppressed
    // shuffling above is done -- without this, the server's stored grid
    // would still list whatever just got equipped/registered out of it
    // (its own restore save was, correctly, also suppressed -- see
    // HandleInventoryRestored) until some unrelated later change happened
    // to trigger a save.
    // Both handlers ignore their parameter and always send the FULL current
    // contents (same "always full, never a diff" contract SendSaveInventory
    // uses) -- the values passed here are irrelevant.
    HandleInventoryChanged();
    HandleEquipmentChangedForSave(EEquipmentSlot::Helmet);
    HandleQuickSlotChangedForSave(0);
}

void AProtoCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 사망으로 인한 레벨 이동이면 위치/시선/무기/인벤토리를 다음 레벨로 이월하지 않는다.
    // 허브에서 PlayerStart + 기본 스탯(체력100/감염0/허기·갈증100)으로 새로 시작한다.
    // (인벤토리는 WipeCarriedInventoryOnDeath에서 이미 비우고 서버에도 빈 상태로 저장됨)
    // bLocalPlayerSetupDone instead of a fresh IsLocallyControlled() call --
    // by this point in a LevelTransition teardown, Controller may already
    // be unpossessed/null (the same "not ready yet" timing IsLocallyControlled()
    // has at the OTHER end of this character's life -- see
    // TrySetupLocalPlayerOnce's header comment), which would silently skip
    // this whole cache and lose the inventory. Once true, it stays true for
    // this character's entire lifetime, so it's a reliable stand-in here.
    if (bLocalPlayerSetupDone && EndPlayReason == EEndPlayReason::LevelTransition && !bIsDead)
    {
        if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
            {
                const FRotator CurrentLook = Controller ? Controller->GetControlRotation() : GetActorRotation();
                NetClient->CacheStateForLevelTransition(
                    GetActorLocation(), CurrentLook, static_cast<uint8>(CurrentWeaponType), BuildInventorySnapshot(),
                    BuildEquipmentSnapshot(), BuildQuickSlotSnapshot());
            }
        }
    }

    Super::EndPlay(EndPlayReason);
}

void AProtoCharacter::AttachCurrentWeaponToSocket(FName SocketName)
{
    if (!CurrentWeapon || !GetMesh())
    {
        return;
    }

    if (!GetMesh()->DoesSocketExist(SocketName))
    {
        if (GEngine)
        {
            const FString Message = FString::Printf(TEXT("No socket: %s"), *SocketName.ToString());
        }
        return;
    }

    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);

    CurrentWeapon->AttachToComponent(GetMesh(), AttachRules, SocketName);
    CurrentWeapon->SetActorEnableCollision(false);
}

void AProtoCharacter::HandleEquipmentChanged(EEquipmentSlot ChangedSlot)
{
    if (ChangedSlot != EEquipmentSlot::Weapon1 && ChangedSlot != EEquipmentSlot::Weapon2)
    {
        return;
    }

    if (AWeaponBase** ExistingPtr = EquippedWeaponActors.Find(ChangedSlot))
    {
        AWeaponBase* ExistingActor = *ExistingPtr;
        if (ExistingActor)
        {
            if (CurrentWeapon == ExistingActor)
            {
                GetWorldTimerManager().ClearTimer(AutoFireTimerHandle);
                bHasWeapon = false;
                CurrentWeapon = nullptr;
                CurrentWeaponType = EWeaponType::None;
                PendingWeaponType = EWeaponType::None;
                Swapping = 0.0f;
                SwappingAlpha = true;
            }

            if (CurrentRifle == ExistingActor)
            {
                CurrentRifle = nullptr;
            }
            if (CurrentPistol == ExistingActor)
            {
                CurrentPistol = nullptr;
            }

            ExistingActor->Destroy();
        }
        EquippedWeaponActors.Remove(ChangedSlot);
    }

    if (!EquipmentComponent)
    {
        return;
    }

    const FEquippedItem& Equipped = EquipmentComponent->GetEquippedItem(ChangedSlot);
    const UWeaponItemData* WeaponItem = Equipped.ItemData ? Cast<UWeaponItemData>(Equipped.ItemData) : nullptr;
    if (!WeaponItem || !WeaponItem->WeaponActorClass || !GetMesh())
    {
        return;
    }

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.Instigator = this;

    AWeaponBase* WeaponActor = GetWorld()->SpawnActor<AWeaponBase>(WeaponItem->WeaponActorClass, GetActorTransform(), SpawnParams);
    if (!WeaponActor)
    {
        return;
    }

    WeaponActor->SetOwner(this);
    WeaponActor->SetInstigator(this);
    WeaponActor->SetActorEnableCollision(false);

    if (UStaticMeshComponent* WeaponMesh = WeaponActor->WeaponMesh)
    {
        WeaponMesh->SetSimulatePhysics(false);
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        WeaponMesh->SetGenerateOverlapEvents(false);
    }
    if (UBoxComponent* CollisionBox = WeaponActor->CollisionBox)
    {
        CollisionBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CollisionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
        CollisionBox->SetGenerateOverlapEvents(false);
    }

    const FName StorageSocketName = WeaponActor->WeaponType == EWeaponType::Pistol ? TEXT("PistolStorage") : TEXT("WeaponStorage");
    const FAttachmentTransformRules AttachRules(
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::SnapToTarget,
        EAttachmentRule::KeepRelative,
        true);
    WeaponActor->AttachToComponent(GetMesh(), AttachRules, StorageSocketName);

    if (WeaponActor->WeaponType == EWeaponType::Rifle)
    {
        CurrentRifle = WeaponActor;
    }
    else if (WeaponActor->WeaponType == EWeaponType::Pistol)
    {
        CurrentPistol = WeaponActor;
    }

    EquippedWeaponActors.Add(ChangedSlot, WeaponActor);
}



void AProtoCharacter::ReloadAmmoAttach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadAmmoAttach(GetMesh());
    }
}

void AProtoCharacter::ReloadAmmoDetach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadAmmoDetach();
    }
}

void AProtoCharacter::ReloadNewAmmo()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadNewAmmo(GetMesh());
    }
}

void AProtoCharacter::ReloadNewAmmoAttach()
{
    if (CurrentWeapon)
    {
        CurrentWeapon->ReloadNewAmmoAttach();
    }
}

void AProtoCharacter::BeginConsumableAnimationState()
{
    bIsUsingConsumable = true;
    bSavedConsumableSwappingAlpha = SwappingAlpha;
    HiddenConsumableWeapon = CurrentWeapon;
    bHiddenConsumableWeaponWasHidden = HiddenConsumableWeapon ? HiddenConsumableWeapon->IsHidden() : false;

    SwappingAlpha = false;

    if (HiddenConsumableWeapon)
    {
        HiddenConsumableWeapon->SetActorHiddenInGame(true);
    }
}

void AProtoCharacter::EndConsumableAnimationState()
{
    if (HiddenConsumableWeapon)
    {
        HiddenConsumableWeapon->SetActorHiddenInGame(bHiddenConsumableWeaponWasHidden);
    }

    HiddenConsumableWeapon = nullptr;
    bHiddenConsumableWeaponWasHidden = false;
    bIsUsingConsumable = false;

    SwappingAlpha = bSavedConsumableSwappingAlpha
        && CurrentWeapon
        && CurrentWeaponType != EWeaponType::None
        && Swapping <= 0.0f
        && !bIsReloading;
}
bool AProtoCharacter::UseConsumable(UConsumableItemData* ConsumableData)
{
    if (!ConsumableData || !StatusComponent) return false;
    if (bIsDead) return false;

    if (bIsUsingConsumable || bIsReloading || Swapping > 0.0f)
    {
        return false;
    }

    // OverTime 효과가 이미 진행 중이면 사용 불가
    if (bOverTimeEffectActive)
    {
        return false;
    }
    
    // 해당 스탯이 만땅(감염도는 0)이면 사용 불가
    switch (ConsumableData->TargetStat)
    {
    case EConsumableTargetStat::Health:
        if (StatusComponent->GetHealth() == StatusComponent->GetMaxHealth())
            return false;
        break;
    case EConsumableTargetStat::Hunger:
        if (StatusComponent->GetHunger() == StatusComponent->GetMaxHunger())
            return false;
        break;
    case EConsumableTargetStat::Thirst:
        if (StatusComponent->GetThirst() == StatusComponent->GetMaxThirst())
            return false;
        break;
    case EConsumableTargetStat::Infection:
        if (StatusComponent->GetInfection() == 0)
            return false;
        break;
    }
    if (RifleReloadMontage)
    {
        FName ConsumableSectionName = NAME_None;
        switch (ConsumableData->TargetStat)
        {
        case EConsumableTargetStat::Health:
            ConsumableSectionName = BandageSectionName;
            break;
        case EConsumableTargetStat::Hunger:
            ConsumableSectionName = EatingSectionName;
            break;
        case EConsumableTargetStat::Thirst:
            ConsumableSectionName = DrinkingSectionName;
            break;
        default:
            break;
        }

        if (ConsumableSectionName != NAME_None)
        {
            BeginConsumableAnimationState();
            const float MontageLength = PlayAnimMontage(RifleReloadMontage, 1.0f, ConsumableSectionName);
            if (MontageLength <= 0.0f)
            {
                EndConsumableAnimationState();
            }
        }
    }

    if (ConsumableData->Application == EEffectApplication::Instant ||
        ConsumableData->Application == EEffectApplication::InstantThenOverTime)
    {
        switch (ConsumableData->TargetStat)
        {
        case EConsumableTargetStat::Health:
            StatusComponent->SetHealth(StatusComponent->GetHealth() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Hunger:
            StatusComponent->SetHunger(StatusComponent->GetHunger() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Thirst:
            StatusComponent->SetThirst(StatusComponent->GetThirst() + ConsumableData->InstantAmount);
            break;
        case EConsumableTargetStat::Infection:
            StatusComponent->SetInfection(StatusComponent->GetInfection() + ConsumableData->InstantAmount);
            break;
        }
    }

    if (ConsumableData->SideEffect.bAppliesDebuff)
    {
        StatusComponent->SetInfection(StatusComponent->GetInfection() + ConsumableData->SideEffect.InfectionIncrease);
    }

    if ((ConsumableData->Application == EEffectApplication::OverTime ||
         ConsumableData->Application == EEffectApplication::InstantThenOverTime) &&
        ConsumableData->OverTimeDurationSeconds > 0.0f)
    {
        bOverTimeEffectActive = true;
        ApplyOverTimeStatEffect(ConsumableData->TargetStat, ConsumableData->OverTimeAmount, ConsumableData->OverTimeDurationSeconds);
    }

    return true;
}

void AProtoCharacter::OnOverTimeEffectFinished()
{
    bOverTimeEffectActive = false;
}

void AProtoCharacter::ApplyOverTimeStatEffect(EConsumableTargetStat TargetStat, float TotalAmount, float Duration)
{
    if (!StatusComponent || Duration <= 0.0f) return;
    

    constexpr float TickInterval = 0.5f;
    const int32 TotalTicks = FMath::Max(1, FMath::RoundToInt(Duration / TickInterval));
    const float AmountPerTick = TotalAmount / TotalTicks;

    TSharedPtr<FTimerHandle> TimerHandle = MakeShared<FTimerHandle>();
    TSharedPtr<int32> RemainingTicks = MakeShared<int32>(TotalTicks);
    TWeakObjectPtr<UPlayerStatusComponent> WeakStatus(StatusComponent);
    TWeakObjectPtr<AProtoCharacter> WeakThis(this);

    FTimerDelegate Delegate;
    Delegate.BindLambda([WeakThis, WeakStatus, TargetStat, AmountPerTick, RemainingTicks, TimerHandle]()
    {
        AProtoCharacter* Character = WeakThis.Get();
        UPlayerStatusComponent* Status = WeakStatus.Get();
        if (!Character || !Status)
        {
            if (Character)
            {
                Character->GetWorldTimerManager().ClearTimer(*TimerHandle);
            }
            return;
        }

        switch (TargetStat)
        {
        case EConsumableTargetStat::Health:
            Status->SetHealth(Status->GetHealth() + AmountPerTick);
            break;
        case EConsumableTargetStat::Hunger:
            Status->SetHunger(Status->GetHunger() + AmountPerTick);
            break;
        case EConsumableTargetStat::Thirst:
            Status->SetThirst(Status->GetThirst() + AmountPerTick);
            break;
        case EConsumableTargetStat::Infection:
            Status->SetInfection(Status->GetInfection() + AmountPerTick);
            break;
        }

        --(*RemainingTicks);
        if (*RemainingTicks <= 0)
        {
            Character->GetWorldTimerManager().ClearTimer(*TimerHandle);
            Character->OnOverTimeEffectFinished();
        }
    });

    GetWorldTimerManager().SetTimer(*TimerHandle, Delegate, TickInterval, true);
}

void AProtoCharacter::OnQuickSlotKeyPressed()
{
    if (bIsInvetoryOpened) return;

    bQuickSlotRadialOpen = false;
    GetWorldTimerManager().SetTimer(QuickSlotHoldTimerHandle, this, &AProtoCharacter::OpenRadialQuickSlotMenu, QuickSlotHoldThreshold, false);
}

void AProtoCharacter::OpenRadialQuickSlotMenu()
{
    if (!RadialQuickSlotWidgetClass || !QuickSlotComponent) return;

    bQuickSlotRadialOpen = true;

    if (!RadialQuickSlotWidgetInstance)
    {
        RadialQuickSlotWidgetInstance = CreateWidget<URadialQuickSlotWidget>(GetWorld(), RadialQuickSlotWidgetClass);
    }
    if (!RadialQuickSlotWidgetInstance) return;

    RadialQuickSlotWidgetInstance->OpenRadial(QuickSlotComponent);
    if (!RadialQuickSlotWidgetInstance->IsInViewport())
    {
        RadialQuickSlotWidgetInstance->AddToViewport(500);
    }

    if (APlayerController* PC = Cast<APlayerController>(Controller))
    {
        PC->SetShowMouseCursor(true);
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(InputMode);
    }
}

void AProtoCharacter::OnQuickSlotKeyReleased()
{
    if (bIsInvetoryOpened) return;

    GetWorldTimerManager().ClearTimer(QuickSlotHoldTimerHandle);

    if (bQuickSlotRadialOpen)
    {
        int32 Selected = INDEX_NONE;
        if (RadialQuickSlotWidgetInstance)
        {
            Selected = RadialQuickSlotWidgetInstance->GetHighlightedSlotIndex();
            RadialQuickSlotWidgetInstance->RemoveFromParent();
        }
        bQuickSlotRadialOpen = false;

        if (APlayerController* PC = Cast<APlayerController>(Controller))
        {
            PC->SetShowMouseCursor(false);
            PC->SetInputMode(FInputModeGameOnly());
        }

        if (Selected != INDEX_NONE && QuickSlotComponent)
        {
            // 꾹 눌러 고르는 동작은 "다음에 쓸 슬롯"을 지정(등록)하는 것뿐이며 그 자리에서 소모하지 않는다.
            // 실제 사용은 이후 4번 키를 짧게 탭했을 때 일어난다.
            QuickSlotComponent->LastUsedSlotIndex = Selected;
            QuickSlotComponent->OnQuickSlotChanged.Broadcast(Selected);
        }
    }
    else if (QuickSlotComponent && QuickSlotComponent->LastUsedSlotIndex != INDEX_NONE)
    {
        QuickSlotComponent->UseQuickSlot(QuickSlotComponent->LastUsedSlotIndex, this);
    }
}



