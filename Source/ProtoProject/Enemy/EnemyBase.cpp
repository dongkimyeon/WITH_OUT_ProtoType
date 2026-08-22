#include "EnemyBase.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Perception/AIPerceptionStimuliSourceComponent.h"
#include "Perception/AISense_Sight.h"

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

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = 300.0f;
    }

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
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    CurrentHealth = MaxHealth;
    BuildBehaviorTree();

    if (PerceptionStimuliSource)
    {
        PerceptionStimuliSource->RegisterForSense(UAISense_Sight::StaticClass());
        PerceptionStimuliSource->RegisterWithPerceptionSystem();
    }
}

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    DebugPrintTimer -= DeltaTime;
    MoveRequestTimer -= DeltaTime;
    UpdateTarget();

    if (BehaviorTreeRoot.IsValid())
    {
        BehaviorTreeRoot->Tick(this, DeltaTime);
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
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
    }

    if (!CanAttack())
    {
        PrintBehaviorDebug(TEXT("Enemy BT: Attack Cooldown"), FColor::Orange);
        return;
    }

    LastAttackTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackTime;
    PrintBehaviorDebug(FString::Printf(TEXT("Enemy BT: Attack / Damage %.1f"), AttackDamage), FColor::Red);
}

void AEnemyBase::MoveToTarget()
{
    if (!HasTarget())
    {
        return;
    }

    if (MoveRequestTimer <= 0.0f)
    {
        MoveRequestTimer = MoveRequestInterval;

        if (AAIController* AIController = Cast<AAIController>(GetController()))
        {
            AIController->MoveToActor(TargetActor, MoveAcceptanceRadius, true, true, true, nullptr, true);
        }
        else
        {
            PrintBehaviorDebug(TEXT("Enemy BT: Move Failed - No AIController"), FColor::Red);
            return;
        }
    }

    PrintBehaviorDebug(TEXT("Enemy BT: MoveToTarget"), FColor::Yellow);
}

void AEnemyBase::Patrol()
{
    PrintBehaviorDebug(TEXT("Enemy BT: Patrol"), FColor::Green);
}

void AEnemyBase::Die()
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;

    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
    }

    PrintBehaviorDebug(TEXT("Enemy BT: Die"), FColor::Silver);
}

void AEnemyBase::TakeEnemyDamage(float DamageAmount)
{
    if (bIsDead || DamageAmount <= 0.0f)
    {
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

void AEnemyBase::BuildBehaviorTree()
{
    TSharedPtr<FEnemySelectorNode> Root = MakeShared<FEnemySelectorNode>();

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

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!IsValid(PlayerPawn))
    {
        TargetActor = nullptr;
        return;
    }

    const float DistanceSquared = FVector::DistSquared(GetActorLocation(), PlayerPawn->GetActorLocation());
    TargetActor = DistanceSquared <= FMath::Square(SightRange) ? PlayerPawn : nullptr;
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




