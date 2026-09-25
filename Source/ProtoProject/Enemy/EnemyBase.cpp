#include "EnemyBase.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NavigationSystem.h"
#include "NavigationData.h"
#include "Navigation/PathFollowingComponent.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"
#include "../PlayerContent/Item/ItemDataBase.h"
#include "../PlayerContent/Item/DropItem.h"
#include "../PlayerContent/ProtoCharacter.h"
#include "../PlayerContent/PlayerStatusComponent.h"
#include "../Companion/CompanionNPC.h"
#include "../Companion/CompanionCombatComponent.h"
#include "../Network/ProtoNetClientSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

using FEnemyBTNode = TBTNode<AEnemyBase>;
using FEnemySelectorNode = TBTSelectorNode<AEnemyBase>;
using FEnemySequenceNode = TBTSequenceNode<AEnemyBase>;
using FEnemyConditionNode = TBTConditionNode<AEnemyBase>;
using FEnemyActionNode = TBTActionNode<AEnemyBase>;
using EEnemyBTResult = EBTNodeResult;

static bool bEnemySoundsEnabled = true;
namespace
{
    constexpr int32 CombatSlotsPerRing = 8;

    struct FEnemyCombatSlotGroup
    {
        TWeakObjectPtr<AActor> Target;
        TMap<int32, TWeakObjectPtr<AEnemyBase>> Claims;
        uint32 Revision = 0;
    };

    TMap<TObjectKey<AActor>, FEnemyCombatSlotGroup> GEnemyCombatSlots;

    TAutoConsoleVariable<int32> CVarCombatSlotPathBudget(TEXT("ai.CombatSlots.MaxPathQueriesPerFrame"), 8,
        TEXT("Maximum slot and chase path requests per world per frame. Minimum 1."));
    TAutoConsoleVariable<int32> CVarCombatSlotDebug(TEXT("ai.CombatSlots.Debug"), 1,
        TEXT("Set to 0 to disable all enemy combat-slot debug drawing."));

    struct FCombatSlotQueryBudget
    {
        struct FWaiter
        {
            TWeakObjectPtr<const AEnemyBase> Enemy;
            uint64 LastRequestFrame = 0;
        };
        uint64 Frame = MAX_uint64;
        int32 Used = 0;
        TMap<TWeakObjectPtr<const AEnemyBase>, int32> PerEnemy;
        TArray<FWaiter> Waiters;

        bool TryConsume(const AEnemyBase* Enemy, uint64 CurrentFrame, int32 Limit)
        {
            if (Frame != CurrentFrame)
            {
                Frame = CurrentFrame;
                Used = 0;
                PerEnemy.Reset();
            }
            Waiters.RemoveAll([CurrentFrame](const FWaiter& Waiter)
            {
                return !Waiter.Enemy.IsValid() || Waiter.Enemy->IsDead() || CurrentFrame - Waiter.LastRequestFrame > 2;
            });
            const TWeakObjectPtr<const AEnemyBase> Key(Enemy);
            const int32 Existing = Waiters.IndexOfByPredicate([Enemy](const FWaiter& Waiter) { return Waiter.Enemy.Get() == Enemy; });
            if (Existing == INDEX_NONE)
            {
                Waiters.Add({ Key, CurrentFrame });
            }
            else
            {
                Waiters[Existing].LastRequestFrame = CurrentFrame;
            }
            const int32 Next = Waiters.IndexOfByPredicate([this](const FWaiter& Waiter) { return PerEnemy.FindRef(Waiter.Enemy) < 2; });
            if (Used >= FMath::Max(1, Limit) || Next == INDEX_NONE || Waiters[Next].Enemy != Key)
            {
                return false;
            }
            Waiters.RemoveAt(Next);
            ++Used;
            ++PerEnemy.FindOrAdd(Key);
            return true;
        }
    };

    struct FWorldCombatSlotBudget
    {
        TWeakObjectPtr<UWorld> World;
        FCombatSlotQueryBudget Budget;
    };
    TMap<TObjectKey<UWorld>, FWorldCombatSlotBudget> GCombatSlotBudgets;

    bool TryConsumeCombatSlotPathBudget(const AEnemyBase* Enemy)
    {
        for (auto It = GCombatSlotBudgets.CreateIterator(); It; ++It)
        {
            if (!It.Value().World.IsValid()) { It.RemoveCurrent(); }
        }
        UWorld* World = Enemy->GetWorld();
        FWorldCombatSlotBudget& Entry = GCombatSlotBudgets.FindOrAdd(TObjectKey<UWorld>(World));
        Entry.World = World;
        return Entry.Budget.TryConsume(Enemy, GFrameCounter, CVarCombatSlotPathBudget.GetValueOnGameThread());
    }

    struct FCombatSlotDebugState
    {
        TWeakObjectPtr<AActor> Target;
        float NextDrawTime = 0.0f;
    };
    TMap<TObjectKey<AActor>, FCombatSlotDebugState> GCombatSlotDebugStates;

    FVector GetCombatSlotDirection(int32 SlotInRing)
    {
        static const FVector Directions[CombatSlotsPerRing] =
        {
            FVector(1.0f, 0.0f, 0.0f),
            FVector(1.0f, 1.0f, 0.0f).GetSafeNormal(),
            FVector(0.0f, 1.0f, 0.0f),
            FVector(-1.0f, 1.0f, 0.0f).GetSafeNormal(),
            FVector(-1.0f, 0.0f, 0.0f),
            FVector(-1.0f, -1.0f, 0.0f).GetSafeNormal(),
            FVector(0.0f, -1.0f, 0.0f),
            FVector(1.0f, -1.0f, 0.0f).GetSafeNormal()
        };

        const int32 ClampedSlot = FMath::Clamp(SlotInRing, 0, CombatSlotsPerRing - 1);
        return Directions[ClampedSlot];
    }

    bool IsCombatSlotHeldByAnother(const TWeakObjectPtr<AEnemyBase>& Claim, const AEnemyBase* Requester)
    {
        AEnemyBase* ClaimOwner = Claim.Get();
        return IsValid(ClaimOwner) && ClaimOwner != Requester && !ClaimOwner->IsDead();
    }
}
AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;

    AIControllerClass = AAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        Capsule->SetCollisionObjectType(ECC_Pawn);
        Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        Capsule->SetGenerateOverlapEvents(true);
    }

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }

    PerceptionStimuliSource = CreateDefaultSubobject<UAIPerceptionStimuliSourceComponent>(TEXT("PerceptionStimuliSource"));

    LeftHandAttackBox = CreateDefaultSubobject<UBoxComponent>(TEXT("LeftHandAttackBox"));
    LeftHandAttackBox->SetupAttachment(GetMesh(), TEXT("RightHand"));
    LeftHandAttackBox->SetBoxExtent(LeftHandAttackBoxExtent);
    LeftHandAttackBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    LeftHandAttackBox->SetCollisionObjectType(ECC_WorldDynamic);
    LeftHandAttackBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    LeftHandAttackBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    LeftHandAttackBox->SetGenerateOverlapEvents(false);
    static ConstructorHelpers::FObjectFinder<USoundBase> IdleSoundFinder(TEXT("/Game/zombieAsset/Sound/ZombieIdle.ZombieIdle"));
    if (IdleSoundFinder.Succeeded())
    {
        IdleSound = IdleSoundFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> AttackSoundFinder(TEXT("/Game/zombieAsset/Sound/ZombieAttack.ZombieAttack"));
    if (AttackSoundFinder.Succeeded())
    {
        AttackSound = AttackSoundFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> ScreamSoundFinder(TEXT("/Game/zombieAsset/Sound/ZombieScream.ZombieScream"));
    if (ScreamSoundFinder.Succeeded())
    {
        ScreamSound = ScreamSoundFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> DieSoundFinder(TEXT("/Game/zombieAsset/Sound/ZombieDie.ZombieDie"));
    if (DieSoundFinder.Succeeded())
    {
        DieSound = DieSoundFinder.Object;
    }
}


void AEnemyBase::ToggleEnemySoundsEnabled()
{
    bEnemySoundsEnabled = !bEnemySoundsEnabled;
}

bool AEnemyBase::AreEnemySoundsEnabled()
{
    return bEnemySoundsEnabled;
}
void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    bUseControllerRotationYaw = false;

    // Apply after Blueprint defaults, including any extra collision components added by enemy variants.
    TInlineComponentArray<UPrimitiveComponent*> CollisionComponents;
    GetComponents(CollisionComponents);
    for (UPrimitiveComponent* Component : CollisionComponents)
    {
        Component->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = MoveSpeed;
        Movement->bUseControllerDesiredRotation = false;
        Movement->bOrientRotationToMovement = bIsNetworkOwner;
        Movement->RotationRate = FRotator(0.0f, FMath::Max(1.0f, MoveTurnRate), 0.0f);
        DefaultMaxAcceleration = Movement->MaxAcceleration;
    }

    BuildBehaviorTree();

    if (LeftHandAttackBox)
    {
        LeftHandAttackBox->SetBoxExtent(LeftHandAttackBoxExtent);
        LeftHandAttackBox->OnComponentBeginOverlap.AddDynamic(this, &AEnemyBase::OnAttackBoxBeginOverlap);
        EndAttackHitWindow();
    }

    if (PerceptionStimuliSource)
    {
        PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
        PerceptionStimuliSource->RegisterWithPerceptionSystem();
    }

    if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
    {
        AnimInstance->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AEnemyBase::HandleAttackMontageNotifyBegin);
        AnimInstance->OnMontageEnded.AddUniqueDynamic(this, &AEnemyBase::HandleAttackMontageEnded);
    }

    // 여러 클라이언트가 각자 이 좀비를 따로 시뮬레이션하지 않도록, 서버에 AI 소유권을
    // 요청한다. 연결 안 된 상태(오프라인 테스트 등)라면 그냥 기존처럼 로컬 AI로 동작한다
    // (bIsNetworkOwner 기본값이 true인 이유).
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->OnEnemyState.AddDynamic(this, &AEnemyBase::HandleEnemyState);
            NetClient->OnEnemyDamage.AddDynamic(this, &AEnemyBase::HandleEnemyDamage);
            NetClient->OnEnemyAttackBroadcast.AddDynamic(this, &AEnemyBase::HandleEnemyAttackBroadcast);

            if (NetClient->IsConnected() && NetClient->IsMultiplayerVisualsEnabled())
            {
                // Multi map: the SERVER drives this enemy's AI directly
                // (see C2S_EnemyRegister's schema comment) -- there's no
                // "granted/denied" round-trip to wait for, this client just
                // switches straight to mirroring whatever S2C_EnemyState
                // reports, same as a denied claim used to do.
                NetClient->SendEnemyRegister(GetEnemyId(), GetActorLocation(), CurrentHealth, MaxHealth, MoveSpeed, AttackRange, AttackDamage, AttackCooldown,
                    IsCallerType(), GetCallRadius(), GetCallCooldown());
                bIsNetworkOwner = false;
            }
            else
            {
                // Single map / offline / not yet connected: fall back to
                // the older per-client ownership-claim path unchanged.
                NetClient->OnEnemyClaimResult.AddDynamic(this, &AEnemyBase::HandleEnemyClaimResult);
                NetClient->OnEnemyOwnerLeft.AddDynamic(this, &AEnemyBase::HandleEnemyOwnerLeft);

                if (NetClient->IsConnected())
                {
                    NetClient->SendEnemyClaimRequest(GetEnemyId());
                }
            }
        }
    }
}


void AEnemyBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ReleaseCombatSlot();
    Super::EndPlay(EndPlayReason);
}
void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Controller yaw must not snap the body or compete with slot/network facing.
    bUseControllerRotationYaw = false;
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->bUseControllerDesiredRotation = false;
        Movement->bOrientRotationToMovement = bIsNetworkOwner && !bIsDead && !bIsAttacking && !bMovementPausedForMontage;
        Movement->RotationRate = FRotator(0.0f, FMath::Max(1.0f, MoveTurnRate), 0.0f);
    }

    if (!bIsNetworkOwner)
    {
        // Another client (or the server) owns this enemy's AI -- no local
        // BT/pathing/targeting to run, just walk toward whatever
        // HandleEnemyState last recorded (see MirroredTargetLocation's
        // comment for why this has to go through CharacterMovementComponent
        // rather than snapping straight there).
        if (bHasMirroredTarget && !bIsDead)
        {
            // Smooth turn instead of snapping straight to the latest
            // ~150ms-old server sample (see MirroredRotationInterpSpeed's
            // comment) -- FMath::RInterpTo takes the shortest path around,
            // so this doesn't spin the long way when Yaw wraps.
            const FRotator NewRotation = FMath::RInterpTo(
                GetActorRotation(), MirroredTargetRotation, DeltaTime, MirroredRotationInterpSpeed);
            const float TurnRate = bIsAttacking || bMovementPausedForMontage ? FacingTurnRate : MoveTurnRate;
            SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), NewRotation, DeltaTime, FMath::Max(1.0f, TurnRate)));

            // Same gate MoveToTarget() already applies for the locally-
            // driven (bIsNetworkOwner) path -- without it, this mirror kept
            // sliding toward MirroredTargetLocation (still catching up from
            // the chase phase, or just re-affirmed every ~150ms by the
            // server's own unchanged-position broadcast while it holds
            // still to swing) at the same time HandleEnemyAttackBroadcast's
            // AttackMontage was playing, i.e. "공격하면서 움직인다": visibly
            // walking/sliding during its own melee swing. The server itself
            // never moves a registered enemy while it's within attackRange
            // (see EnemyAI::Tick's else-branch) -- this mirrors that same
            // "holding position to attack" rule client-side, using the
            // exact bIsAttacking flag HandleEnemyAttackBroadcast already
            // maintains for the animation.
            if (!bIsAttacking && !bMovementPausedForMontage)
            {
                FVector ToTarget = MirroredTargetLocation - GetActorLocation();
                ToTarget.Z = 0.0f; // horizontal input only; let gravity/step-up handle height

                constexpr float ArrivalToleranceCm = 5.0f;
                if (ToTarget.SizeSquared() > FMath::Square(ArrivalToleranceCm))
                {
                    AddMovementInput(ToTarget.GetSafeNormal(), 1.0f, /*bForce=*/true);
                }
            }
        }
        return;
    }

    DebugPrintTimer -= DeltaTime;
    MoveRequestTimer -= DeltaTime;
    CombatSlotReclaimBlockTimer -= DeltaTime;
    CombatSlotRecheckTimer -= DeltaTime;
    CombatSlotProjectionTimer -= DeltaTime;
    UpdateTarget();
    UpdateCombatSlotClaim();
    UpdateCombatSlotStuck(DeltaTime);

    if (BehaviorTreeRoot.IsValid())
    {
        BehaviorTreeRoot->Tick(this, DeltaTime);
    }
    DrawCombatSlotsDebug();

    NetSyncTimer -= DeltaTime;
    if (NetSyncTimer <= 0.0f)
    {
        NetSyncTimer = NetSyncInterval;
        if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
            {
                NetClient->SendEnemyState(GetEnemyId(), GetActorLocation(), GetActorRotation(), CurrentHealth, bIsDead);
            }
        }
    }
}

bool AEnemyBase::HasTarget() const
{
    return IsValid(TargetActor);
}

bool AEnemyBase::IsTargetInAttackRange() const
{
    if (!HasTarget())
    {
        return false;
    }

    return FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(AttackRange);
}

bool AEnemyBase::CanAttack() const
{
    if (bIsDead || bMovementPausedForMontage || !HasTarget() || !IsTargetInAttackRange() || !IsCombatSlotAttackReady())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    return World->GetTimeSeconds() - LastAttackTime >= AttackCooldown;
}

void AEnemyBase::Attack()
{
    if (bIsAttacking)
    {
        return;
    }

    if (!CanAttack())
    {
        PrintBehaviorDebug(TEXT("Enemy BT: Attack Cooldown"), FColor::Orange);
        return;
    }

    LastAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackTime;
    PrintBehaviorDebug(FString::Printf(TEXT("Enemy BT: Attack / Damage %.1f"), AttackDamage), FColor::Red);

    if (AttackMontage)
    {
        bIsAttacking = PlayAnimMontage(AttackMontage) > 0.0f;
        if (bIsAttacking)
        {
            PlayEnemySound(AttackSound);
        }
        if (bIsAttacking)
        {
            PauseMovementForMontage(AttackMontage);
        }
    }
}

void AEnemyBase::PauseMovementForMontage(UAnimMontage* Montage)
{
    if (!Montage || bIsDead)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (!bMovementPausedForMontage)
        {
            SavedMontageMaxWalkSpeed = Movement->MaxWalkSpeed;
            SavedMontageMaxAcceleration = Movement->MaxAcceleration;
        }

        bMovementPausedForMontage = true;
        MovementPausedMontage = Montage;
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
        Movement->StopMovementImmediately();
        Movement->MaxWalkSpeed = 0.0f;
        Movement->MaxAcceleration = 0.0f;
    }
}

void AEnemyBase::RestoreMovementAfterMontage(UAnimMontage* Montage)
{
    if (!bMovementPausedForMontage || (Montage && MovementPausedMontage && Montage != MovementPausedMontage))
    {
        return;
    }

    bMovementPausedForMontage = false;
    MovementPausedMontage = nullptr;

    if (!bIsDead)
    {
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->MaxWalkSpeed = SavedMontageMaxWalkSpeed > 0.0f ? SavedMontageMaxWalkSpeed : MoveSpeed;
            Movement->MaxAcceleration = SavedMontageMaxAcceleration > 0.0f ? SavedMontageMaxAcceleration : DefaultMaxAcceleration;
        }
    }
}

void AEnemyBase::HandleAttackMontageNotifyBegin(FName NotifyName, const FBranchingPointNotifyPayload& BranchingPointPayload)
{
    static const FName BeginAttackNotifyName(TEXT("BeginAttack"));
    static const FName EndAttackNotifyName(TEXT("EndAttack"));

    if (NotifyName == BeginAttackNotifyName)
    {
        BeginAttackHitWindow();
    }
    else if (NotifyName == EndAttackNotifyName)
    {
        EndAttackHitWindow();
    }
}

void AEnemyBase::HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    if (!AttackMontage || Montage == AttackMontage)
    {
        bIsAttacking = false;
        EndAttackHitWindow();
    }

    RestoreMovementAfterMontage(Montage);
}

void AEnemyBase::BeginAttackHitWindow()
{
    DamagedActorsThisSwing.Reset();

    if (!LeftHandAttackBox || bIsDead)
    {
        return;
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = AttackMoveSpeed;
        Movement->MaxAcceleration = AttackAcceleration;
    }

    LeftHandAttackBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    LeftHandAttackBox->SetGenerateOverlapEvents(true);
}

void AEnemyBase::EndAttackHitWindow()
{
    if (!LeftHandAttackBox)
    {
        return;
    }

    LeftHandAttackBox->SetGenerateOverlapEvents(false);
    LeftHandAttackBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    if (!bMovementPausedForMontage && !bIsDead)
    {
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->MaxWalkSpeed = MoveSpeed;
            Movement->MaxAcceleration = DefaultMaxAcceleration;
        }
    }

    DamagedActorsThisSwing.Reset();
}

void AEnemyBase::OnAttackBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor || OtherActor == this || bIsDead || DamagedActorsThisSwing.Contains(OtherActor))
    {
        return;
    }

    if (!bIsNetworkOwner)
    {
        // Not locally driving this enemy -- its attack box never actually
        // gets positioned by a real attack montage on this copy (Tick()
        // skips the whole local BT/Attack() path), so this should never
        // fire in practice. Explicit guard anyway: for a server-driven
        // (Multi map) enemy, player damage comes exclusively through the
        // server's own S2C_EnemyAttackResult now (see
        // AProtoCharacter::HandleEnemyAttackPlayer), never straight from a
        // local overlap this client isn't the authority for.
        return;
    }

    bool bAppliedDamage = false;

    if (AProtoCharacter* PlayerCharacter = Cast<AProtoCharacter>(OtherActor))
    {
        if (UPlayerStatusComponent* StatusComponent = PlayerCharacter->GetStatusComponent())
        {
            StatusComponent->SetHealth(StatusComponent->GetHealth() - AttackDamage);
            bAppliedDamage = true;
        }
    }
    else if (ACompanionNPC* Companion = Cast<ACompanionNPC>(OtherActor))
    {
        if (Companion->CombatComponent)
        {
            Companion->CombatComponent->TakeCompanionDamage(AttackDamage);
            bAppliedDamage = true;
        }
    }

    if (bAppliedDamage)
    {
        DamagedActorsThisSwing.Add(OtherActor);
        PrintBehaviorDebug(FString::Printf(TEXT("Enemy BT: Melee Hit %s / Damage %.1f"), *OtherActor->GetName(), AttackDamage), FColor::Red);
    }
}
void AEnemyBase::UpdateCombatSlotClaim()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Enemy_CombatSlotUpdate);
    if (CombatSlotSearchTarget.Get() != TargetActor)
    {
        ReleaseCombatSlot();
        CombatSlotSearchTarget = TargetActor;
        BlockedCombatSlots.Empty();
        CombatSlotReclaimBlockTimer = 0.0f;
        CombatSlotRecheckTimer = FMath::FRandRange(0.0f, 0.15f);
        CombatSlotWaitStartedAt = GetWorld()->GetTimeSeconds();
    }

    if (!CanUseCombatSlotsForCurrentTarget())
    {
        ReleaseCombatSlot();
        CombatSlotStatus = TEXT("Direct chase");
        return;
    }
    if (bIsAttacking || bMovementPausedForMontage)
    {
        CombatSlotStatus = bIsAttacking ? TEXT("Attacking") : TEXT("Montage");
        return;
    }
    if (HasCombatSlot())
    {
        // Track a moving target at the movement-request rate without running a full path search every frame.
        if (CombatSlotProjectionTimer <= 0.0f)
        {
            CombatSlotProjectionTimer = FMath::Max(0.05f, MoveRequestInterval);
            const FVector RawLocation = GetRawCombatSlotLocation(CombatSlotIndex);
            if (!RawLocation.Equals(CombatSlotLastProjectionInput, 1.0f) ||
                GetWorld()->GetTimeSeconds() - CombatSlotLastProjectionTime >= FMath::Max(0.1f, SlotIdleRecheckInterval))
            {
                FVector UpdatedLocation;
                if (!ProjectCombatSlotLocation(RawLocation, UpdatedLocation))
                {
                    RejectCombatSlot(TEXT("Slot left navigation"));
                    return;
                }
                CombatSlotLocation = UpdatedLocation;
                CombatSlotLastProjectionInput = RawLocation;
                CombatSlotLastProjectionTime = GetWorld()->GetTimeSeconds();
            }
        }
        UpdateCombatSlotArrival();
        if (bCombatSlotArrived && CombatSlotIndex >= CombatSlotsPerRing)
        {
            const FEnemyCombatSlotGroup* Group = GEnemyCombatSlots.Find(TObjectKey<AActor>(TargetActor));
            if (Group && Group->Revision != CombatSlotObservedRevision)
            {
                CombatSlotObservedRevision = Group->Revision;
                CombatSlotRecheckTimer = 0.0f;
            }
        }
    }
    if (CombatSlotReclaimBlockTimer > 0.0f || CombatSlotRecheckTimer > 0.0f)
    {
        return;
    }
    const float RecheckInterval = bCombatSlotArrived ? SlotIdleRecheckInterval : SlotRecheckInterval;
    CombatSlotRecheckTimer = FMath::Max(0.1f, RecheckInterval) * FMath::FRandRange(0.85f, 1.15f);
    const float Now = GetWorld()->GetTimeSeconds();
    for (auto It = BlockedCombatSlots.CreateIterator(); It; ++It)
    {
        if (It.Value() <= Now)
        {
            It.RemoveCurrent();
        }
    }
    if (HasCombatSlot())
    {
        FVector UpdatedLocation;
        if (!EvaluateCombatSlot(CombatSlotIndex, UpdatedLocation))
        {
            RejectCombatSlot(TEXT("Slot blocked / no complete path"));
            return;
        }
        CombatSlotLocation = UpdatedLocation;
        UpdateCombatSlotArrival();
        if (bCombatSlotArrived && CombatSlotIndex >= CombatSlotsPerRing)
        {
            TryClaimCombatSlot(true);
        }
    }
    else
    {
        TryClaimCombatSlot();
    }
}

void AEnemyBase::ReleaseCombatSlot()
{
    ResetCombatSlotSearch();
    bCombatSlotMoveDeferred = false;
    AActor* PreviousTarget = CombatSlotTarget.Get();
    if (!PreviousTarget || CombatSlotIndex == INDEX_NONE)
    {
        CombatSlotTarget.Reset();
        CombatSlotIndex = INDEX_NONE;
        bCombatSlotArrived = false;
        ResetCombatSlotStuckTracking();
        return;
    }

    if (FEnemyCombatSlotGroup* Group = GEnemyCombatSlots.Find(TObjectKey<AActor>(PreviousTarget)))
    {
        ++Group->Revision;
        for (auto It = Group->Claims.CreateIterator(); It; ++It)
        {
            if (!It.Value().IsValid() || It.Value().Get() == this)
            {
                It.RemoveCurrent();
            }
        }

        if (Group->Claims.Num() == 0)
        {
            GEnemyCombatSlots.Remove(TObjectKey<AActor>(PreviousTarget));
        }
    }

    CombatSlotTarget.Reset();
    CombatSlotIndex = INDEX_NONE;
    bCombatSlotArrived = false;
    ResetCombatSlotStuckTracking();
}

bool AEnemyBase::HasCombatSlot() const
{
    return CombatSlotTarget.IsValid() && CombatSlotTarget.Get() == TargetActor && CombatSlotIndex != INDEX_NONE;
}

float AEnemyBase::GetCombatSlotRingRadius(int32 Ring) const
{
    const float InnerRadius = FMath::Min(SlotFirstRingRadius, FMath::Max(0.0f, AttackRange - SlotAttackRangePadding));
    return InnerRadius + SlotRingSpacing * FMath::Max(0, Ring);
}

bool AEnemyBase::ProjectCombatSlotLocation(const FVector& RawLocation, FVector& OutLocation) const
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!NavSystem)
    {
        return false;
    }

    FNavLocation ProjectedLocation;
    const FVector QueryExtent(SlotNavigationProjectionExtent, SlotNavigationProjectionExtent, SlotNavigationProjectionExtent);
    if (!NavSystem->ProjectPointToNavigation(RawLocation, ProjectedLocation, QueryExtent, &GetNavAgentPropertiesRef()))
    {
        return false;
    }

    OutLocation = ProjectedLocation.Location;
    return true;
}

FVector AEnemyBase::GetCombatSlotLocation() const
{
    if (!HasCombatSlot())
    {
        return TargetActor ? TargetActor->GetActorLocation() : GetActorLocation();
    }

    return CombatSlotLocation;
}

FVector AEnemyBase::GetRawCombatSlotLocation(int32 Index) const
{
    FVector Direction = FVector::ZeroVector;
    const int32 Footprint = FMath::Clamp(SlotFootprint, 1, CombatSlotsPerRing);
    for (int32 Offset = 0; Offset < Footprint; ++Offset)
    {
        Direction += GetCombatSlotDirection((Index + Offset) % CombatSlotsPerRing);
    }
    if (Direction.IsNearlyZero())
    {
        Direction = GetCombatSlotDirection(Index % CombatSlotsPerRing);
    }
    const APawn* TargetPawn = Cast<APawn>(TargetActor);
    const FVector Center = TargetPawn ? TargetPawn->GetNavAgentLocation() : TargetActor->GetActorLocation();
    return Center + Direction.GetSafeNormal() * GetCombatSlotRingRadius(Index / CombatSlotsPerRing);
}

bool AEnemyBase::FindCombatSlotPath(const FVector& Location, float& OutLength, bool* bOutDeferred) const
{
    if (bOutDeferred) { *bOutDeferred = false; }
    UNavigationSystemV1* NavSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    const ANavigationData* NavData = NavSystem ? NavSystem->GetNavDataForProps(GetNavAgentPropertiesRef(), GetNavAgentLocation()) : nullptr;
    if (!NavData)
    {
        return false;
    }
    if (!TryConsumeCombatSlotPathBudget(this))
    {
        if (bOutDeferred) { *bOutDeferred = true; }
        return false;
    }
    TRACE_CPUPROFILER_EVENT_SCOPE(Enemy_CombatSlotPathQuery);
    FPathFindingQuery Query(GetController(), *NavData, GetNavAgentLocation(), Location);
    Query.SetAllowPartialPaths(false);
    const FPathFindingResult Result = NavSystem->FindPathSync(GetNavAgentPropertiesRef(), Query);
    if (!Result.IsSuccessful() || !Result.Path.IsValid() || Result.Path->IsPartial())
    {
        return false;
    }
    OutLength = Result.Path->GetLength();
    return true;
}

bool AEnemyBase::EvaluateCombatSlot(int32 Index, FVector& OutLocation) const
{
    if (!HasTarget())
    {
        return false;
    }
    const int32 Ring = Index / CombatSlotsPerRing;
    const FEnemyCombatSlotGroup* Group = GEnemyCombatSlots.Find(TObjectKey<AActor>(TargetActor));
    for (int32 Offset = 0; Offset < FMath::Clamp(SlotFootprint, 1, CombatSlotsPerRing); ++Offset)
    {
        const int32 Cell = Ring * CombatSlotsPerRing + (Index + Offset) % CombatSlotsPerRing;
        if (BlockedCombatSlots.FindRef(Cell) > GetWorld()->GetTimeSeconds() ||
            (Group && IsCombatSlotHeldByAnother(Group->Claims.FindRef(Cell), this)))
        {
            return false;
        }
    }
    if (!ProjectCombatSlotLocation(GetRawCombatSlotLocation(Index), OutLocation))
    {
        return false;
    }
    // Projection must not push an attack position beyond melee reach.
    if (Ring == 0 && FVector::Dist2D(OutLocation, TargetActor->GetActorLocation()) >
        FMath::Max(0.0f, AttackRange - FMath::Max(5.0f, SlotAttackRangePadding * 0.5f)))
    {
        return false;
    }
    const float Radius = GetCapsuleComponent()->GetScaledCapsuleRadius();
    if (Ring == 0)
    {
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySlotCandidate), false, this);
        Params.AddIgnoredActor(TargetActor);
        const FVector AttackOrigin = OutLocation + FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
        if (GetWorld()->LineTraceSingleByChannel(Hit, AttackOrigin, TargetActor->GetActorLocation(), ECC_Visibility, Params))
        {
            return false;
        }
    }
    if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
    {
        if (FVector::Dist2D(OutLocation, TargetActor->GetActorLocation()) <
            Radius + TargetCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius())
        {
            return false;
        }
    }
    // Distinct slot indices can project onto the same narrow piece of navmesh.
    if (Group)
    {
        for (const auto& Claim : Group->Claims)
        {
            const AEnemyBase* Other = Claim.Value.Get();
            if (Other && Other != this && !Other->IsDead() && Other->HasCombatSlot())
            {
                const float Separation = Radius + Other->GetCapsuleComponent()->GetScaledCapsuleRadius() + 5.0f;
                if (FVector::DistSquared2D(OutLocation, Other->GetCombatSlotLocation()) < FMath::Square(Separation))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

void AEnemyBase::ResetCombatSlotSearch()
{
    CombatSlotCandidates.Reset();
    CombatSlotCandidateCursor = 0;
    CombatSlotBestCandidate = INDEX_NONE;
    CombatSlotBestScore = TNumericLimits<float>::Max();
    bCombatSlotSearchPending = false;
}

bool AEnemyBase::TryClaimCombatSlot(bool bInnerOnly)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(Enemy_CombatSlotSelection);
    if (!CanUseCombatSlotsForCurrentTarget()) { return false; }
    if (HasCombatSlot() && !bInnerOnly) { return true; }

    const FVector TargetLocation = TargetActor->GetActorLocation();
    const float FrontPreference = FMath::Max(0.0f, SlotFrontPreference);
    const auto FrontCost = [&](const FVector& Location)
    {
        const FVector Direction = (Location - TargetLocation).GetSafeNormal2D();
        const float Dot = FMath::Clamp(FVector::DotProduct(CombatSlotSearchForward, Direction), -1.0f, 1.0f);
        return FrontPreference * (1.0f - Dot) * 0.5f;
    };
    if (bCombatSlotSearchPending && (bCombatSlotSearchInnerOnly != bInnerOnly ||
        FVector::DistSquared(TargetLocation, CombatSlotSearchTargetOrigin) > FMath::Square(SlotRepathDistance) ||
        FVector::DistSquared(GetActorLocation(), CombatSlotSearchOrigin) > FMath::Square(SlotRepathDistance)))
    {
        ResetCombatSlotSearch();
    }
    if (!bCombatSlotSearchPending)
    {
        ResetCombatSlotSearch();
        bCombatSlotSearchPending = true;
        bCombatSlotSearchInnerOnly = bInnerOnly;
        CombatSlotSearchOrigin = GetActorLocation();
        CombatSlotSearchTargetOrigin = TargetLocation;
        CombatSlotSearchForward = TargetActor->GetActorForwardVector().GetSafeNormal2D();
        const int32 RingCount = bInnerOnly ? CombatSlotIndex / CombatSlotsPerRing : FMath::Max(1, MaxSlotRings);
        for (int32 Index = 0; Index < RingCount * CombatSlotsPerRing; ++Index)
        {
            FVector Location;
            if (EvaluateCombatSlot(Index, Location))
            {
                CombatSlotCandidates.Add({ Index, Location,
                    static_cast<float>(FVector::Dist2D(GetActorLocation(), Location)) + FrontCost(Location) });
            }
        }
        // Order cheap estimates first, while preserving the inner-ring priority.
        CombatSlotCandidates.Sort([](const FSlotSearchCandidate& A, const FSlotSearchCandidate& B)
        {
            const int32 RingA = A.Index / CombatSlotsPerRing;
            const int32 RingB = B.Index / CombatSlotsPerRing;
            if (RingA != RingB) { return RingA < RingB; }
            return A.LowerBound == B.LowerBound ? A.Index < B.Index : A.LowerBound < B.LowerBound;
        });
    }

    while (CombatSlotCandidateCursor < CombatSlotCandidates.Num())
    {
        const FSlotSearchCandidate& Candidate = CombatSlotCandidates[CombatSlotCandidateCursor];
        if (CombatSlotBestCandidate != INDEX_NONE && Candidate.Index / CombatSlotsPerRing > CombatSlotBestCandidate / CombatSlotsPerRing)
        {
            break;
        }
        FVector Location;
        if (!EvaluateCombatSlot(Candidate.Index, Location))
        {
            ++CombatSlotCandidateCursor;
            continue;
        }
        const float Penalty = FrontCost(Location);
        if (FVector::Dist2D(GetActorLocation(), Location) + Penalty >= CombatSlotBestScore)
        {
            ++CombatSlotCandidateCursor;
            continue;
        }
        // An older waiter can use its own proven candidate; never pathfind on its behalf.
        bool bOlderWaiter = false;
        if (const FEnemyCombatSlotGroup* Group = GEnemyCombatSlots.Find(TObjectKey<AActor>(TargetActor)))
        {
            for (const auto& Claim : Group->Claims)
            {
                const AEnemyBase* Other = Claim.Value.Get();
                if (Other && Other != this && !Other->IsDead() && Other->bCombatSlotArrived &&
                    Other->bCombatSlotSearchPending && Other->CombatSlotBestCandidate == Candidate.Index &&
                    Other->CombatSlotWaitStartedAt < CombatSlotWaitStartedAt)
                {
                    bOlderWaiter = true;
                    break;
                }
            }
        }
        if (bOlderWaiter) { ++CombatSlotCandidateCursor; continue; }
        float Length = 0.0f;
        bool bDeferred = false;
        const bool bReachable = FindCombatSlotPath(Location, Length, &bDeferred);
        if (bDeferred)
        {
            // Preserve all progress and reservations; exhausted CPU budget is not a blocked path.
            CombatSlotRecheckTimer = 0.0f;
            CombatSlotStatus = TEXT("Waiting: path query budget");
            return false;
        }
        ++CombatSlotCandidateCursor;
        if (bReachable && Length + Penalty < CombatSlotBestScore)
        {
            CombatSlotBestCandidate = Candidate.Index;
            CombatSlotBestScore = Length + Penalty;
            CombatSlotBestPathLength = Length;
            CombatSlotBestLocation = Location;
        }
    }

    if (CombatSlotBestCandidate == INDEX_NONE)
    {
        ResetCombatSlotSearch();
        if (!HasCombatSlot()) { CombatSlotStatus = TEXT("Waiting: no reachable free slot"); }
        return false;
    }
    FVector FinalLocation;
    if (!EvaluateCombatSlot(CombatSlotBestCandidate, FinalLocation) ||
        FVector::DistSquared(FinalLocation, CombatSlotBestLocation) > FMath::Square(SlotRepathDistance))
    {
        ResetCombatSlotSearch();
        CombatSlotRecheckTimer = 0.0f;
        return false;
    }
    const int32 ChosenIndex = CombatSlotBestCandidate;
    const float ChosenPathLength = CombatSlotBestPathLength;
    ReleaseCombatSlot();
    FEnemyCombatSlotGroup& Group = GEnemyCombatSlots.FindOrAdd(TObjectKey<AActor>(TargetActor));
    Group.Target = TargetActor;
    CombatSlotTarget = TargetActor;
    CombatSlotIndex = ChosenIndex;
    CombatSlotLocation = FinalLocation;
    CombatSlotLastProjectionInput = GetRawCombatSlotLocation(ChosenIndex);
    CombatSlotLastProjectionTime = GetWorld()->GetTimeSeconds();
    CombatSlotProjectionTimer = FMath::Max(0.05f, MoveRequestInterval);
    bCombatSlotArrived = false;
    MoveRequestTimer = 0.0f;
    CombatSlotStatus = TEXT("Approaching slot");
    ResetCombatSlotStuckTracking();
    CombatSlotLastDistance = ChosenPathLength;
    for (int32 Offset = 0; Offset < FMath::Clamp(SlotFootprint, 1, CombatSlotsPerRing); ++Offset)
    {
        const int32 Cell = ChosenIndex / CombatSlotsPerRing * CombatSlotsPerRing + (ChosenIndex + Offset) % CombatSlotsPerRing;
        Group.Claims.Add(Cell, this);
    }
    CombatSlotObservedRevision = ++Group.Revision;
    return true;
}

bool AEnemyBase::CanUseCombatSlotsForCurrentTarget() const
{
    if (!bUseCombatSlots || bIsDead || !HasTarget())
    {
        return false;
    }

    const float ClaimDistance = HasCombatSlot() ? FMath::Max(SlotClaimDistance,
        GetCombatSlotRingRadius(CombatSlotIndex / CombatSlotsPerRing) + SlotDepartureMargin) : SlotClaimDistance;
    return FVector::DistSquared2D(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(ClaimDistance);
}

float AEnemyBase::GetCombatSlotArrivalRadius() const
{
    // Keep the approach tolerance inside the first ring's attack-range margin.
    return CombatSlotIndex < CombatSlotsPerRing
        ? FMath::Min(SlotMoveAcceptanceRadius, FMath::Max(2.0f, SlotAttackRangePadding * 0.25f))
        : SlotMoveAcceptanceRadius;
}

void AEnemyBase::UpdateCombatSlotArrival()
{
    const float Distance = FVector::Dist2D(GetActorLocation(), GetCombatSlotLocation());
    const bool bInnerSlot = CombatSlotIndex < CombatSlotsPerRing;
    if (bCombatSlotArrived)
    {
        if (Distance > GetCombatSlotArrivalRadius() + SlotDepartureMargin ||
            (bInnerSlot && !IsTargetInAttackRange()))
        {
            bCombatSlotArrived = false;
            MoveRequestTimer = 0.0f;
            ResetCombatSlotStuckTracking();
        }
    }
    else if (Distance <= GetCombatSlotArrivalRadius())
    {
        bCombatSlotArrived = true;
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
    }
    CombatSlotStatus = bCombatSlotArrived
        ? (bInnerSlot ? TEXT("Ready: facing / range / cooldown") : TEXT("Waiting: inner slot occupied"))
        : TEXT("Approaching slot");
}

bool AEnemyBase::IsCombatSlotAttackReady() const
{
    if (!CanUseCombatSlotsForCurrentTarget())
    {
        return true;
    }
    if (!HasCombatSlot() || CombatSlotIndex >= CombatSlotsPerRing || !bCombatSlotArrived)
    {
        return false;
    }
    const FVector Direction = (TargetActor->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
    if (FVector::DotProduct(GetActorForwardVector().GetSafeNormal2D(), Direction) <
        FMath::Cos(FMath::DegreesToRadians(SlotAttackFacingAngle)))
    {
        return false;
    }
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemySlotAttack), false, this);
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, GetActorLocation(),
        TargetActor->GetActorLocation(), ECC_Visibility, Params);
    return !bBlocked || Hit.GetActor() == TargetActor;
}

void AEnemyBase::RejectCombatSlot(const TCHAR* Reason)
{
    if (HasCombatSlot())
    {
        const float RetryAt = GetWorld()->GetTimeSeconds() + FMath::Max(0.1f, SlotBlockedRetryDelay);
        for (int32 Offset = 0; Offset < FMath::Clamp(SlotFootprint, 1, CombatSlotsPerRing); ++Offset)
        {
            const int32 Cell = CombatSlotIndex / CombatSlotsPerRing * CombatSlotsPerRing +
                (CombatSlotIndex + Offset) % CombatSlotsPerRing;
            BlockedCombatSlots.Add(Cell, RetryAt);
        }
    }
    ReleaseCombatSlot();
    CombatSlotReclaimBlockTimer = SlotReclaimDelay;
    CombatSlotRecheckTimer = 0.0f;
    MoveRequestTimer = 0.0f;
    CombatSlotStatus = Reason;
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
    }
    PrintBehaviorDebug(FString::Printf(TEXT("Enemy Slot: %s"), Reason), FColor::Orange);
}

void AEnemyBase::UpdateCombatSlotStuck(float DeltaTime)
{
    if (!HasCombatSlot() || bCombatSlotArrived || bCombatSlotMoveDeferred || bIsDead || bIsAttacking || bMovementPausedForMontage)
    {
        ResetCombatSlotStuckTracking();
        return;
    }

    CombatSlotStuckTimer += DeltaTime;
    if (CombatSlotStuckTimer < FMath::Max(0.1f, SlotStuckTimeout))
    {
        return;
    }
    // Position change also counts as progress on a curved path; do not pathfind just to detect a stall.
    const float RemainingLength = FVector::Dist2D(GetActorLocation(), GetCombatSlotLocation());
    const bool bMadeProgress = CombatSlotLastDistance >= 0.0f &&
        CombatSlotLastDistance - RemainingLength > SlotStuckProgressTolerance;
    const bool bChangedPosition = FVector::Dist2D(GetActorLocation(), CombatSlotProgressLocation) > SlotStuckProgressTolerance;
    const bool bMovingSlowly = GetVelocity().Size2D() <= SlotStuckVelocityThreshold;
    if (!bMadeProgress && (!bChangedPosition || bMovingSlowly))
    {
        RejectCombatSlot(TEXT("Stuck: slot temporarily excluded"));
        return;
    }
    CombatSlotStuckTimer = 0.0f;
    CombatSlotLastDistance = RemainingLength;
    CombatSlotProgressLocation = GetActorLocation();
}

void AEnemyBase::ResetCombatSlotStuckTracking()
{
    CombatSlotStuckTimer = 0.0f;
    CombatSlotLastDistance = -1.0f;
    CombatSlotProgressLocation = GetActorLocation();
}

void AEnemyBase::DrawCombatSlotsDebug() const
{
    if (!bDrawCombatSlots || CVarCombatSlotDebug.GetValueOnGameThread() == 0 || !bUseCombatSlots || bIsDead || !HasTarget())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    const float Now = World->GetTimeSeconds();
    if (Now < CombatSlotNextDebugTime) { return; }
    TRACE_CPUPROFILER_EVENT_SCOPE(Enemy_CombatSlotDebug);
    const float Interval = FMath::Max(0.05f, CombatSlotDebugInterval);
    CombatSlotNextDebugTime = Now + Interval;
    for (auto It = GCombatSlotDebugStates.CreateIterator(); It; ++It)
    {
        if (!It.Value().Target.IsValid()) { It.RemoveCurrent(); }
    }

    const TObjectKey<AActor> TargetKey(TargetActor);
    const FEnemyCombatSlotGroup* Group = GEnemyCombatSlots.Find(TargetKey);
    const int32 RingCount = FMath::Max(1, MaxSlotRings);
    const FVector DebugOffset(0.0f, 0.0f, CombatSlotDebugZOffset);

    FCombatSlotDebugState& DebugState = GCombatSlotDebugStates.FindOrAdd(TargetKey);
    DebugState.Target = TargetActor;
    // The grid is shared by the target, and its geometry stays visible until the next refresh.
    if (Now >= DebugState.NextDrawTime)
    {
        DebugState.NextDrawTime = Now + Interval;
        const APawn* TargetPawn = Cast<APawn>(TargetActor);
        const FVector Center = TargetPawn ? TargetPawn->GetNavAgentLocation() : TargetActor->GetActorLocation();
        for (int32 Ring = 0; Ring < RingCount; ++Ring)
        {
            for (int32 SlotInRing = 0; SlotInRing < CombatSlotsPerRing; ++SlotInRing)
            {
                const int32 SlotIndex = Ring * CombatSlotsPerRing + SlotInRing;
                const FVector RawSlotLocation = Center + GetCombatSlotDirection(SlotInRing) * GetCombatSlotRingRadius(Ring);
                FVector SlotLocation;
                const bool bHasNavSlot = ProjectCombatSlotLocation(RawSlotLocation, SlotLocation);
                SlotLocation = (bHasNavSlot ? SlotLocation : RawSlotLocation) + DebugOffset;
                FColor SlotColor = bHasNavSlot ? FColor::Green : FColor::Silver;
                if (Group)
                {
                    AEnemyBase* ClaimOwner = Group->Claims.FindRef(SlotIndex).Get();
                    if (IsValid(ClaimOwner)) { SlotColor = FColor::Red; }
                }
                DrawDebugSphere(World, SlotLocation, CombatSlotDebugSphereRadius, 8, SlotColor, false, Interval, 0, 2.0f);
            }
        }
    }
    if (HasCombatSlot())
    {
        DrawDebugLine(World, GetActorLocation(), GetCombatSlotLocation() + DebugOffset, FColor::Cyan, false, Interval, 0, 2.0f);
        DrawDebugSphere(World, GetCombatSlotLocation() + DebugOffset, GetCombatSlotArrivalRadius(), 8, FColor::Cyan, false, Interval);
    }
    const FString Label = FString::Printf(TEXT("Slot %d | %s"), CombatSlotIndex, *CombatSlotStatus);
    DrawDebugString(World, GetActorLocation() + FVector(0.0f, 0.0f, 110.0f), Label, nullptr, FColor::White, Interval, true);
}
void AEnemyBase::MoveToTarget()
{
    if (bMovementPausedForMontage || bIsAttacking || !HasTarget())
    {
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
        return;
    }

    const bool bSlotMode = CanUseCombatSlotsForCurrentTarget();
    if (bSlotMode && (!HasCombatSlot() || bCombatSlotArrived))
    {
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->bOrientRotationToMovement = false;
        }
        const FRotator Facing(0.0f, (TargetActor->GetActorLocation() - GetActorLocation()).Rotation().Yaw, 0.0f);
        const float DeltaTime = GetWorld()->GetDeltaSeconds();
        const FRotator SmoothedFacing = FMath::RInterpTo(GetActorRotation(), Facing, DeltaTime, SlotFacingInterpSpeed);
        SetActorRotation(FMath::RInterpConstantTo(GetActorRotation(), SmoothedFacing, DeltaTime, FMath::Max(1.0f, FacingTurnRate)));
        if (HasCombatSlot() && CombatSlotIndex < CombatSlotsPerRing)
        {
            CombatSlotStatus = !IsTargetInAttackRange() ? TEXT("Out of attack range") :
                (!IsCombatSlotAttackReady() ? TEXT("Turning / attack obstructed") : TEXT("Attack cooldown"));
        }
        return;
    }

    if (MoveRequestTimer <= 0.0f)
    {
        MoveRequestTimer = MoveRequestInterval;

        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            const bool bSlotMove = HasCombatSlot();
            const FVector Goal = bSlotMove ? GetCombatSlotLocation() : TargetActor->GetActorLocation();
            const bool bSameRequest = CombatSlotLastMoveTarget.Get() == TargetActor && bCombatSlotLastMoveWasSlot == bSlotMove &&
                (!bSlotMove || FVector::DistSquared(Goal, CombatSlotLastMoveGoal) <= FMath::Square(SlotRepathDistance));
            if (bSameRequest && AIController->GetMoveStatus() == EPathFollowingStatus::Moving)
            {
                bCombatSlotMoveDeferred = false;
                return;
            }
            if (!TryConsumeCombatSlotPathBudget(this))
            {
                bCombatSlotMoveDeferred = true;
                MoveRequestTimer = 0.0f;
                return;
            }
            bCombatSlotMoveDeferred = false;
            CombatSlotLastMoveTarget = TargetActor;
            bCombatSlotLastMoveWasSlot = bSlotMove;
            CombatSlotLastMoveGoal = Goal;
            if (HasCombatSlot())
            {
                const EPathFollowingRequestResult::Type Result = AIController->MoveToLocation(
                    GetCombatSlotLocation(), GetCombatSlotArrivalRadius(), false, true, false, false, nullptr, false);
                if (Result == EPathFollowingRequestResult::Failed)
                {
                    RejectCombatSlot(TEXT("Move request failed"));
                }
            }
            else
            {
                AIController->MoveToActor(TargetActor, MoveAcceptanceRadius, false, true, true, nullptr, true);
            }
        }
        else
        {
            PrintBehaviorDebug(TEXT("Enemy BT: Move Failed - No AIController"), FColor::Red);
            return;
        }
    }

    PrintBehaviorDebug(HasCombatSlot() ? TEXT("Enemy BT: MoveToCombatSlot") : TEXT("Enemy BT: MoveToTarget"), FColor::Yellow);
}

void AEnemyBase::PlayEnemySound(USoundBase* Sound) const
{
    if (!bEnemySoundsEnabled || !Sound)
    {
        return;
    }

    UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), EnemySoundVolume, EnemySoundPitch);
}

void AEnemyBase::PlayIdleSoundIfReady()
{
    if (!IdleSound || bIsDead || bIsAttacking || HasTarget())
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    if (World->GetTimeSeconds() - LastIdleSoundTime >= IdleSoundInterval)
    {
        LastIdleSoundTime = World->GetTimeSeconds();
        PlayEnemySound(IdleSound);
    }
}
void AEnemyBase::Patrol()
{
    PlayIdleSoundIfReady();
    PrintBehaviorDebug(TEXT("Enemy BT: Patrol"), FColor::Green);
}

void AEnemyBase::Die()
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;
    ReleaseCombatSlot();
    PlayEnemySound(DieSound);
    bIsAttacking = false;
    EndAttackHitWindow();

    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
        AIController->UnPossess();
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->DisableMovement();
        Movement->StopMovementImmediately();
    }

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        MeshComponent->SetCollisionProfileName(TEXT("Ragdoll"));
        MeshComponent->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
        MeshComponent->SetSimulatePhysics(bEnableRagdollOnDeath);
        MeshComponent->WakeAllRigidBodies();
    }

    SpawnLoot();

    if (RagdollLifeSpan > 0.0f)
    {
        SetLifeSpan(RagdollLifeSpan);
    }

    PrintBehaviorDebug(TEXT("Enemy BT: Die"), FColor::Silver);
}

void AEnemyBase::SpawnLoot()
{
    if (LootTable.Num() == 0 || FMath::FRand() > LootDropChance)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    UItemDataBase* ChosenItem = LootTable[FMath::RandRange(0, LootTable.Num() - 1)];
    if (!ChosenItem)
    {
        return;
    }

    FTransform SpawnTransform = GetActorTransform();
    SpawnTransform.SetLocation(SpawnTransform.GetLocation() + FVector(0.f, 0.f, 30.f));

    // ItemData는 OnConstruction이 메시를 붙이는 데 쓰이므로, 스폰 후 대입하면 늦다 -
    // Deferred 스폰으로 ItemData를 먼저 세팅한 뒤 FinishSpawning에서 OnConstruction이 돌게 한다.
    ADropItem* Drop = World->SpawnActorDeferred<ADropItem>(ADropItem::StaticClass(), SpawnTransform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Drop)
    {
        return;
    }

    Drop->ItemData = ChosenItem;
    Drop->StackCount = 1;
    Drop->FinishSpawning(SpawnTransform);
}

void AEnemyBase::TakeEnemyDamage(float DamageAmount)
{
    if (bIsDead || DamageAmount <= 0.0f)
    {
        return;
    }

    if (!bIsNetworkOwner)
    {
        // Not this enemy's health authority -- relay the hit to whichever
        // client is, instead of applying it to our own copy (which the
        // next S2C_EnemyState would just overwrite anyway). See
        // HandleEnemyDamage on the receiving end.
        if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
            {
                NetClient->SendEnemyDamage(GetEnemyId(), DamageAmount);
            }
        }
        return;
    }

    CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
    PrintBehaviorDebug(FString::Printf(TEXT("Enemy BT: Hit %.1f / HP %.1f"), DamageAmount, CurrentHealth), FColor::Orange);

    if (CurrentHealth <= 0.0f)
    {
        Die();
    }
}

void AEnemyBase::OnHit(float DamageAmount)
{
    TakeEnemyDamage(DamageAmount);
}

int32 AEnemyBase::GetEnemyId() const
{
    return static_cast<int32>(GetTypeHash(GetName()));
}

void AEnemyBase::HandleEnemyClaimResult(int32 EnemyId, bool bGranted)
{
    if (EnemyId != GetEnemyId())
    {
        return; // Every enemy in the level shares this broadcast delegate.
    }

    bIsNetworkOwner = bGranted;

    if (!bGranted)
    {
        ReleaseCombatSlot();
        // Someone else already owns this enemy's AI -- stop ours so it
        // doesn't fight the network updates HandleEnemyState is about to
        // start applying. Mirrors what Die() does to movement, minus the
        // ragdoll/loot side effects (this enemy isn't dead, just not
        // locally driven anymore).
        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->StopMovement();
        }
        if (UCharacterMovementComponent* Movement = GetCharacterMovement())
        {
            Movement->DisableMovement();
        }
    }
}

void AEnemyBase::HandleEnemyState(int32 EnemyId, FVector Position, FRotator Look, float Health, bool bIsDeadState)
{
    // The server never echoes C2S_EnemyState back to its own sender, so
    // !bIsNetworkOwner is implied here -- checked anyway as cheap defense
    // in depth against ever fighting our own local simulation.
    if (EnemyId != GetEnemyId() || bIsNetworkOwner)
    {
        return;
    }

    MirroredTargetRotation = FRotator(0.0f, Look.Yaw, 0.0f);
    MirroredTargetLocation = Position;

    if (!bHasMirroredTarget)
    {
        // First update ever received for this enemy: snap instead of
        // walking there. This instance was spawned at its LEVEL-PLACED
        // position (unlike a remote player/companion puppet, which is
        // spawned fresh at the network-reported spot -- see
        // UProtoNetClientSubsystem::UpdateRemoteCompanion), so if this
        // client joined mid-session after the enemy had already wandered
        // far from that spawn point, walking the whole distance here would
        // look like it sprinting cross-map to catch up. Every update after
        // this one goes through Tick()'s smooth walk instead (see
        // MirroredTargetLocation's comment).
        SetActorLocationAndRotation(Position, MirroredTargetRotation);
        bHasMirroredTarget = true;
    }

    CurrentHealth = Health;

    if (bIsDeadState && !bIsDead)
    {
        Die();
    }
}

void AEnemyBase::HandleEnemyOwnerLeft(int32 EnemyId)
{
    if (EnemyId != GetEnemyId() || bIsNetworkOwner || bIsDead)
    {
        return;
    }

    // Whoever was driving this enemy disconnected -- try to pick it up
    // ourselves so it doesn't stay frozen in place forever.
    if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (UProtoNetClientSubsystem* NetClient = GameInstance->GetSubsystem<UProtoNetClientSubsystem>())
        {
            NetClient->SendEnemyClaimRequest(EnemyId);
        }
    }
}

void AEnemyBase::HandleEnemyDamage(int32 EnemyId, float Damage)
{
    // The server only ever relays this to the current owner, but check
    // anyway -- every enemy in the level shares this broadcast delegate.
    if (EnemyId != GetEnemyId() || !bIsNetworkOwner)
    {
        return;
    }

    TakeEnemyDamage(Damage);
}

void AEnemyBase::HandleEnemyAttackBroadcast(int32 EnemyId)
{
    // Every enemy in the level shares this broadcast delegate. bIsNetworkOwner
    // is redundant in practice (the server only ever sends this for
    // registered/server-driven enemy ids, which by construction have no
    // owning client at all -- see the header comment) but kept as cheap
    // defense-in-depth, same as HandleEnemyState/HandleEnemyDamage's own checks.
    if (EnemyId != GetEnemyId() || bIsNetworkOwner || bIsDead || bIsAttacking)
    {
        return;
    }

    // Play only -- no CanAttack()/cooldown/target gating here, the server
    // already decided this swing happens. HandleAttackMontageNotifyBegin/
    // HandleAttackMontageEnded (bound unconditionally in BeginPlay) drive
    // bIsAttacking/the hit window exactly as they do for a locally-driven
    // attack; OnAttackBoxBeginOverlap is a no-op here anyway since
    // !bIsNetworkOwner, so this is animation-only, never a second source of damage.
    if (AttackMontage)
    {
        bIsAttacking = PlayAnimMontage(AttackMontage) > 0.0f;
    }
}

void AEnemyBase::BuildBehaviorTree()
{
    TSharedPtr<FEnemySelectorNode> Root = MakeShared<FEnemySelectorNode>();

    // Caller 타입 전용: 주변 좀비를 불러모으는 부수효과만 내고 항상 Failed를 반환해
    // 같은 틱에 아래의 Attack/Move 시퀀스로 자연스럽게 넘어간다.
    TSharedPtr<FEnemySequenceNode> CallSequence = MakeShared<FEnemySequenceNode>();
    CallSequence->Children.Add(MakeShared<FEnemyConditionNode>([](AEnemyBase* Enemy)
    {
        return IsValid(Enemy) && Enemy->CanCall();
    }));
    CallSequence->Children.Add(MakeShared<FEnemyActionNode>([](AEnemyBase* Enemy, float DeltaTime)
    {
        Enemy->DoCall();
        return EEnemyBTResult::Failed;
    }));

    TSharedPtr<FEnemySequenceNode> AttackSequence = MakeShared<FEnemySequenceNode>();
    AttackSequence->Children.Add(MakeShared<FEnemyConditionNode>([](AEnemyBase* Enemy)
    {
        return IsValid(Enemy) && !Enemy->bIsDead && Enemy->HasTarget();
    }));
    AttackSequence->Children.Add(MakeShared<FEnemyConditionNode>([](AEnemyBase* Enemy)
    {
        return IsValid(Enemy) && Enemy->CanAttack();
    }));
    AttackSequence->Children.Add(MakeShared<FEnemyActionNode>([](AEnemyBase* Enemy, float DeltaTime)
    {
        Enemy->Attack();
        return EEnemyBTResult::Running;
    }));

    TSharedPtr<FEnemySequenceNode> MoveSequence = MakeShared<FEnemySequenceNode>();
    MoveSequence->Children.Add(MakeShared<FEnemyConditionNode>([](AEnemyBase* Enemy)
    {
        return IsValid(Enemy) && !Enemy->bIsDead && Enemy->HasTarget();
    }));
    MoveSequence->Children.Add(MakeShared<FEnemyActionNode>([](AEnemyBase* Enemy, float DeltaTime)
    {
        Enemy->MoveToTarget();
        return EEnemyBTResult::Running;
    }));

    Root->Children.Add(CallSequence);
    Root->Children.Add(AttackSequence);
    Root->Children.Add(MoveSequence);
    Root->Children.Add(MakeShared<FEnemyActionNode>([](AEnemyBase* Enemy, float DeltaTime)
    {
        if (IsValid(Enemy) && !Enemy->bIsDead)
        {
            Enemy->Patrol();
            return EEnemyBTResult::Running;
        }

        return EEnemyBTResult::Failed;
    }));

    BehaviorTreeRoot = Root;
}

void AEnemyBase::UpdateTarget()
{
    if (bIsDead)
    {
        TargetActor = nullptr;
        return;
    }

    // 이미 Caller에게 불려와 강제로 세팅된 타겟이 아직 사거리 안이면 그대로 유지한다.
    if (IsValid(TargetActor) &&
        FVector::DistSquared(GetActorLocation(), TargetActor->GetActorLocation()) <= FMath::Square(SightRange))
    {
        return;
    }

    const float SightRangeSquared = FMath::Square(SightRange);
    AActor* BestTarget = nullptr;
    float BestDistanceSquared = SightRangeSquared;

    if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), PlayerPawn->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared && CanSeeCandidate(PlayerPawn))
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = PlayerPawn;
        }
    }

    TArray<AActor*> Companions;
    UGameplayStatics::GetAllActorsOfClass(this, ACompanionNPC::StaticClass(), Companions);
    for (AActor* CompanionActor : Companions)
    {
        const ACompanionNPC* Companion = Cast<ACompanionNPC>(CompanionActor);
        if (!Companion || (Companion->CombatComponent && Companion->CombatComponent->IsDead()))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(GetActorLocation(), CompanionActor->GetActorLocation());
        if (DistanceSquared <= BestDistanceSquared && CanSeeCandidate(CompanionActor))
        {
            BestDistanceSquared = DistanceSquared;
            BestTarget = CompanionActor;
        }
    }

    if (TargetActor != BestTarget)
    {
        ReleaseCombatSlot();
    }

    TargetActor = BestTarget;
}

bool AEnemyBase::CanSeeCandidate(const AActor* Candidate) const
{
    if (!IsValid(Candidate))
    {
        return false;
    }

    // 시야각: 정면 벡터와 후보 방향 사이 각도가 SightAngle의 절반보다 크면 시야 밖.
    const FVector ToCandidate = (Candidate->GetActorLocation() - GetActorLocation()).GetSafeNormal();
    const float DotToCandidate = FVector::DotProduct(GetActorForwardVector(), ToCandidate);
    const float HalfAngleRad = FMath::DegreesToRadians(SightAngle * 0.5f);
    if (DotToCandidate < FMath::Cos(HalfAngleRad))
    {
        return false;
    }

    // 장애물 차단(라인오브사이트): 벽 등에 가로막혀 있으면 시야 밖으로 취급한다.
    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector EyeLocation = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    const FVector CandidateLocation = Candidate->GetActorLocation() + FVector(0.f, 0.f, 50.f);

    FCollisionQueryParams Params(TEXT("EnemySight"), false, this);
    Params.AddIgnoredActor(Candidate);

    FHitResult Hit;
    const bool bBlocked = World->LineTraceSingleByChannel(Hit, EyeLocation, CandidateLocation, ECC_Visibility, Params);
    return !bBlocked;
}

bool AEnemyBase::CanCall() const
{
    return false;
}

void AEnemyBase::DoCall()
{
}

void AEnemyBase::ReceiveCallTarget(AActor* NewTarget)
{
    if (bIsDead)
    {
        return;
    }

    if (TargetActor != NewTarget)
    {
        ReleaseCombatSlot();
    }

    TargetActor = NewTarget;
}

void AEnemyBase::PrintBehaviorDebug(const FString& Message, const FColor& Color)
{
    if (!bEnableBehaviorDebug)
    {
        return;
    }

    LastBehaviorDebugMessage = Message;
    if (DebugPrintTimer > 0.0f)
    {
        return;
    }

    DebugPrintTimer = DebugPrintInterval;


    UKismetSystemLibrary::PrintString(this, Message, true, false, Color, DebugPrintInterval);
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatSlotBudgetTest, "ProtoProject.Enemy.CombatSlotBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatSlotBudgetTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreateNavigation(false).CreateAISystem(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Budget test world"), World)) { return false; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEnemyBase* A = World->SpawnActor<AEnemyBase>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    AEnemyBase* B = World->SpawnActor<AEnemyBase>(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
    AEnemyBase* C = World->SpawnActor<AEnemyBase>(FVector(1000.0f, 0.0f, 0.0f), FRotator::ZeroRotator, Params);
    if (!TestNotNull(TEXT("Enemy A"), A) || !TestNotNull(TEXT("Enemy B"), B) || !TestNotNull(TEXT("Enemy C"), C))
    {
        World->DestroyWorld(false);
        return false;
    }
    FCombatSlotQueryBudget Budget;
    TestTrue(TEXT("First query permitted"), Budget.TryConsume(A, 100, 3));
    TestTrue(TEXT("Second query permitted"), Budget.TryConsume(A, 100, 3));
    TestFalse(TEXT("One enemy cannot consume more than two queries in a frame"), Budget.TryConsume(A, 100, 3));
    TestTrue(TEXT("Another enemy can use remaining capacity"), Budget.TryConsume(B, 100, 3));
    TestFalse(TEXT("World limit defers further queries"), Budget.TryConsume(C, 100, 3));
    TestEqual(TEXT("No budget overrun"), Budget.Used, 3);
    TestTrue(TEXT("Frame advance replenishes budget"), Budget.TryConsume(A, 101, 3));
    TestFalse(TEXT("Earlier queued enemy gets its turn before a repeat caller"), Budget.TryConsume(A, 101, 3));
    TestTrue(TEXT("Queued enemy progresses"), Budget.TryConsume(C, 101, 3));
    TestTrue(TEXT("Deferred repeat caller then progresses"), Budget.TryConsume(A, 101, 3));
    TestFalse(TEXT("Further waiter is queued"), Budget.TryConsume(B, 101, 3));
    TestTrue(TEXT("Inactive waiters cannot block the queue forever"), Budget.TryConsume(A, 105, 3));

    A->bEnableBehaviorDebug = false;
    A->TargetActor = B;
    A->CombatSlotTarget = B;
    A->CombatSlotIndex = 0;
    A->CombatSlotLocation = FVector(200.0f, 0.0f, 0.0f);
    A->bCombatSlotMoveDeferred = true;
    A->UpdateCombatSlotStuck(10.0f);
    TestTrue(TEXT("Budget wait must retain the reserved slot"), A->HasCombatSlot());
    TestTrue(TEXT("Budget wait must not blacklist a valid slot"), A->BlockedCombatSlots.IsEmpty());
    A->bCombatSlotMoveDeferred = false;
    A->UpdateCombatSlotStuck(A->SlotStuckTimeout + 0.1f);
    TestFalse(TEXT("Actual immobility still releases the slot without pathfinding"), A->HasCombatSlot());
    TestTrue(TEXT("Actual immobility still excludes the failed slot"), A->BlockedCombatSlots.Contains(0));
    A->ReleaseCombatSlot();
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCombatSlotStateTest, "ProtoProject.Enemy.CombatSlotState",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEnemyCombatSlotStateTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Init = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreateNavigation(false).CreateAISystem(false).CreatePhysicsScene(true).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
    if (!TestNotNull(TEXT("Test world"), World))
    {
        return false;
    }
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AEnemyBase* Enemy = World->SpawnActor<AEnemyBase>(FVector(120.0f, 0.0f, 100.0f), FRotator(0.0f, 180.0f, 0.0f), SpawnParams);
    ACharacter* Target = World->SpawnActor<ACharacter>(FVector(0.0f, 0.0f, 100.0f), FRotator::ZeroRotator, SpawnParams);
    if (!TestNotNull(TEXT("Enemy"), Enemy) || !TestNotNull(TEXT("Target"), Target))
    {
        World->DestroyWorld(false);
        return false;
    }
    Enemy->bEnableBehaviorDebug = false;
    Enemy->TargetActor = Target;
    Enemy->CombatSlotSearchTarget = Target;
    Enemy->AttackRange = 150.0f;
    TestFalse(TEXT("In range without a reserved slot must not attack"), Enemy->CanAttack());
    float PathLength = 0.0f;
    TestFalse(TEXT("Missing navigation must not count as a complete path"), Enemy->FindCombatSlotPath(Target->GetActorLocation(), PathLength));

    Enemy->CombatSlotTarget = Target;
    Enemy->CombatSlotIndex = CombatSlotsPerRing;
    Enemy->CombatSlotLocation = Enemy->GetNavAgentLocation();
    Enemy->bCombatSlotArrived = true;
    Enemy->CombatSlotRecheckTimer = 10.0f;
    Enemy->CombatSlotProjectionTimer = 10.0f;
    auto& Group = GEnemyCombatSlots.FindOrAdd(TObjectKey<AActor>(Target));
    Group.Target = Target;
    Group.Claims.Add(CombatSlotsPerRing, Enemy);
    Enemy->UpdateCombatSlotClaim();
    TestTrue(TEXT("An outer reservation is retained, not released for being outside attack radius"), Enemy->HasCombatSlot());
    TestFalse(TEXT("An outer waiter cannot attack even if the target comes into range"), Enemy->CanAttack());

    Enemy->ReleaseCombatSlot();
    TestFalse(TEXT("Releasing the last reservation removes its group"), GEnemyCombatSlots.Contains(TObjectKey<AActor>(Target)));
    Enemy->CombatSlotTarget = Target;
    Enemy->CombatSlotIndex = 0;
    Enemy->CombatSlotLocation = Enemy->GetNavAgentLocation();
    Enemy->UpdateCombatSlotArrival();
    TestTrue(TEXT("Arrived inner enemy facing the target can attack"), Enemy->CanAttack());
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallCollision = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(WallCollision);
    WallCollision->SetBoxExtent(FVector(10.0f, 50.0f, 100.0f));
    WallCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    WallCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
    WallCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    WallCollision->RegisterComponent();
    Wall->SetActorLocation(FVector(60.0f, 0.0f, 100.0f));
    TestFalse(TEXT("A wall between a ready attacker and target prevents attacking"), Enemy->CanAttack());
    WallCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Enemy->SetActorRotation(FRotator::ZeroRotator);
    TestFalse(TEXT("Facing away blocks the attack"), Enemy->CanAttack());
    Enemy->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
    Enemy->LastAttackTime = World->GetTimeSeconds();
    TestFalse(TEXT("A ready slot does not bypass cooldown"), Enemy->CanAttack());
    Enemy->LastAttackTime = -999.0f;

    Enemy->SetActorLocation(FVector(135.0f, 0.0f, 100.0f));
    Enemy->UpdateCombatSlotArrival();
    TestTrue(TEXT("Small displacement retains arrival state"), Enemy->bCombatSlotArrived);
    Enemy->SetActorLocation(FVector(190.0f, 0.0f, 100.0f));
    Enemy->UpdateCombatSlotArrival();
    TestFalse(TEXT("Leaving range restarts approach"), Enemy->bCombatSlotArrived);

    Enemy->SlotFootprint = 2;
    Enemy->CombatSlotIndex = 7;
    auto& WrappedGroup = GEnemyCombatSlots.FindOrAdd(TObjectKey<AActor>(Target));
    WrappedGroup.Claims.Add(7, Enemy);
    WrappedGroup.Claims.Add(0, Enemy);
    const FVector BeforeTurn = Enemy->GetRawCombatSlotLocation(7);
    Target->SetActorRotation(FRotator(0.0f, 90.0f, 0.0f));
    TestTrue(TEXT("Target rotation does not rotate slots"), BeforeTurn.Equals(Enemy->GetRawCombatSlotLocation(7)));
    Enemy->RejectCombatSlot(TEXT("Test blockage"));
    TestFalse(TEXT("Blocked reservation is released"), Enemy->HasCombatSlot());
    TestTrue(TEXT("Both cells of a wrapped footprint are temporarily excluded"),
        Enemy->BlockedCombatSlots.FindRef(7) > World->GetTimeSeconds() && Enemy->BlockedCombatSlots.FindRef(0) > World->GetTimeSeconds());
    TestFalse(TEXT("Wrapped reservations leave no stale ownership"), GEnemyCombatSlots.Contains(TObjectKey<AActor>(Target)));

    Enemy->TargetActor = nullptr;
    Enemy->UpdateCombatSlotClaim();
    TestTrue(TEXT("Changing targets clears old slot exclusions"), Enemy->BlockedCombatSlots.IsEmpty());
    Enemy->ReleaseCombatSlot();
    World->DestroyWorld(false);
    return true;
}
#endif












