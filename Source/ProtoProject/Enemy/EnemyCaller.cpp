#include "EnemyCaller.h"

#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

AEnemyCaller::AEnemyCaller()
{
    MoveSpeed = 300.0f;
}

void AEnemyCaller::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bEnableBehaviorDebug || !bShowCallRadius)
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        // 콜리전 박스/스피어를 실제로 추가하지 않고(부르는 범위는 게임플레이 판정용 콜라이더가
        // 아니라 DoCall()의 거리 체크일 뿐이라) 디버그 스피어로만 시각화한다 - 매 틱 다시 그려서
        // CallRadius를 에디터/PIE에서 실시간으로 조정해도 즉시 반영되게 한다.
        const FColor SphereColor = CanCall() ? FColor::Green : FColor::Red;
        DrawDebugSphere(World, GetActorLocation(), CallRadius, 24, SphereColor, false, -1.0f, 0, 2.0f);
    }
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

    if (CallMontage)
    {
        if (PlayAnimMontage(CallMontage, 1.0f, CallSectionName) > 0.0f)
        {
            PlayEnemySound(ScreamSound);
            PauseMovementForMontage(CallMontage);
        }
    }

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


