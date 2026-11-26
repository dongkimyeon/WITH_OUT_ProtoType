#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "EnemyImpactComponent.generated.h"

class USkeletalMeshComponent;
class UProceduralMeshComponent;
class UPhysicalAnimationComponent;
class UMaterialInterface;

// Cosmetic only: never modifies health, AI, inventory, or network state.
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class ENEMYCOMBATFX_API UEnemyImpactComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UEnemyImpactComponent();
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UFUNCTION(BlueprintCallable, Category="Enemy FX")
    void PlayHitReaction(const FHitResult& Hit, FVector ShotDirection);

    UFUNCTION(BlueprintCallable, Category="Enemy FX")
    void NotifyDeath();

    UFUNCTION(BlueprintCallable, Category="Enemy FX")
    bool SliceCorpse(FVector PlanePosition, FVector PlaneNormal, FVector Impulse);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit")
    bool bEnableHitReaction = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit", meta=(ClampMin="0"))
    float HitImpulse = 180.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit", meta=(ClampMin="0.05"))
    float HitDuration = 0.3f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit", meta=(ClampMin="0", ClampMax="1"))
    float MaxPhysicsBlend = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit")
    FName FallbackHitBone = TEXT("spine_01");
    // Limits simulation to the upper body; leg hits are remapped to the fallback.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit")
    FName HitReactionRoot = TEXT("spine_01");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Hit", meta=(ClampMin="0"))
    float OrientationStrength = 1000.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice")
    bool bEnableCorpseSlicing = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice")
    TObjectPtr<UMaterialInterface> CapMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice", meta=(ClampMin="1"))
    int32 MaxSliceTriangles = 60000;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice", meta=(ClampMin="0.1"))
    float SliceCooldown = 0.25f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice", meta=(ClampMin="1", ClampMax="16"))
    int32 MaxPieces = 4;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Enemy FX|Slice", meta=(ClampMin="0"))
    float PieceLifetime = 12.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy FX|Debug")
    FString LastResult;

private:
    friend class AEnemySliceTestActor;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;
    UPROPERTY(Transient) TObjectPtr<UPhysicalAnimationComponent> PhysicalAnimation;
    UPROPERTY(Transient) TArray<TObjectPtr<UProceduralMeshComponent>> Pieces;
    TMap<FName, float> ActiveReactions;
    bool bDead = false;
    ECollisionEnabled::Type PreviousCollision = ECollisionEnabled::QueryOnly;
    float LastSliceTime = -1000.0f;
    UProceduralMeshComponent* CapturePose();
    void ConfigurePiece(UProceduralMeshComponent* Piece, const FVector& Velocity, const FVector& Impulse);
    void Report(const FString& Message);
};

// Asset-free test target. Place this actor or use enemyfx.SpawnTest.
UCLASS()
class ENEMYCOMBATFX_API AEnemySliceTestActor : public AActor
{
    GENERATED_BODY()
public:
    AEnemySliceTestActor();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> TestMesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UEnemyImpactComponent> Impact;
};
