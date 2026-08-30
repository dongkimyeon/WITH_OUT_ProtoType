#include "EnemyCaller.h"

#include "Kismet/GameplayStatics.h"

AEnemyCaller::AEnemyCaller()
{
    MoveSpeed = 300.0f;
}

bool AEnemyCaller::CanCall() const
{
    if (IsDead() || !HasTarget())
    {
        return false;
    }

    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    return World->GetTimeSeconds() - LastCallTime >= CallCooldown;
}

void AEnemyCaller::DoCall()
{
    LastCallTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastCallTime;

    TArray<AActor*> NearbyEnemies;
    UGameplayStatics::GetAllActorsOfClass(this, AEnemyBase::StaticClass(), NearbyEnemies);

    for (AActor* Actor : NearbyEnemies)
    {
        AEnemyBase* OtherEnemy = Cast<AEnemyBase>(Actor);
        if (!OtherEnemy || OtherEnemy == this || OtherEnemy->IsDead() || OtherEnemy->HasTarget())
        {
            continue;
        }

        if (FVector::DistSquared(GetActorLocation(), OtherEnemy->GetActorLocation()) <= FMath::Square(CallRadius))
        {
            OtherEnemy->ReceiveCallTarget(TargetActor);
        }
    }

    PrintBehaviorDebug(TEXT("Enemy BT: Call nearby zombies"), FColor::Magenta);
}
