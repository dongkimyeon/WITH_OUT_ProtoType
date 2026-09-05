#include "EnemyBase.h"

#include "AIController.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
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

using FEnemyBTNode = TBTNode<AEnemyBase>;
using FEnemySelectorNode = TBTSelectorNode<AEnemyBase>;
using FEnemySequenceNode = TBTSequenceNode<AEnemyBase>;
using FEnemyConditionNode = TBTConditionNode<AEnemyBase>;
using FEnemyActionNode = TBTActionNode<AEnemyBase>;
using EEnemyBTResult = EBTNodeResult;

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
        Capsule->SetGenerateOverlapEvents(true);
    }

    if (USkeletalMeshComponent* MeshComponent = GetMesh())
    {
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        MeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
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

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = MoveSpeed;
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

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsNetworkOwner)
    {
        // Another client owns this enemy's AI -- HandleEnemyState applies
        // its reported position/health/death directly as each update
        // arrives, so there's no local BT/pathing/targeting to run here.
        return;
    }

    DebugPrintTimer -= DeltaTime;
    MoveRequestTimer -= DeltaTime;
    UpdateTarget();

    if (BehaviorTreeRoot.IsValid())
    {
        BehaviorTreeRoot->Tick(this, DeltaTime);
    }

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
    if (bIsDead || !HasTarget() || !IsTargetInAttackRange())
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

    if (MoveRequestTimer <= 0.0f)
    {
        MoveRequestTimer = MoveRequestInterval;

        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->MoveToActor(TargetActor, MoveAcceptanceRadius, false, true, true, nullptr, true);
        }
        else
        {
            PrintBehaviorDebug(TEXT("Enemy BT: Move Failed - No AIController"), FColor::Red);
            return;
        }
    }

    PrintBehaviorDebug(TEXT("Enemy BT: MoveToTarget"), FColor::Yellow);
}


void AEnemyBase::PlayEnemySound(USoundBase* Sound) const
{
    if (Sound)
    {
        UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), EnemySoundVolume, EnemySoundPitch);
    }
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

    SetActorLocationAndRotation(Position, FRotator(0.0f, Look.Yaw, 0.0f));
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
        return IsValid(Enemy) && Enemy->IsTargetInAttackRange();
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












