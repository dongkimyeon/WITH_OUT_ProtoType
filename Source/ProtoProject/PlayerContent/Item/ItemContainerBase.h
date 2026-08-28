#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "../Interactable.h"
#include "ItemContainerBase.generated.h"

class UStaticMeshComponent;
class UInventoryGridComponent;
class AProtoCharacter;

UCLASS(Abstract, meta = (PrioritizeCategories = "Container"))
class PROTOPROJECT_API AItemContainerBase : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AItemContainerBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInventoryGridComponent* ContainerInventory;

	UPROPERTY(EditAnywhere, Category = "Container")
	FText ContainerName;

	virtual void OnInteract_Implementation(AProtoCharacter* InPlayer) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AProtoCharacter* InPlayer) const override;

	// Stable id shared across every client's copy of this level, derived
	// from the placed actor's own in-level name rather than assigned by the
	// server -- as long as everyone loaded the same packaged Content, the
	// same container gets the same id on every machine with no handshake
	// needed. Used to key the server-authoritative loot roll (see
	// UProtoNetClientSubsystem::SendContainerLootRoll / OnContainerLootState).
	UFUNCTION(BlueprintCallable, Category = "Container")
	int32 GetContainerId() const;

protected:
	virtual void BeginPlay() override;

	// BeginPlay에서 오버랩 바인딩 직후 1회 호출된다. 베이스는 아무것도 안 채운다(빈 채로 시작).
	virtual void SeedContents() {}

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
