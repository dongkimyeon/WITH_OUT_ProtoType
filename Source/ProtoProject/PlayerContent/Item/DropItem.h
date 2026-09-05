#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "ItemDataBase.h"
#include "../Interactable.h"
#include "DropItem.generated.h"

class UStaticMeshComponent;
class AProtoCharacter;
class UInventoryGridComponent;

UCLASS(meta = (PrioritizeCategories = "Data"))
class PROTOPROJECT_API ADropItem : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	ADropItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* BoundingBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractBox;

	UPROPERTY(EditDefaultsOnly, Category = "Data")
	UItemDataBase* ItemData;

	
	UPROPERTY(EditAnywhere, Category = "Data")
	int32 StackCount = 1;

	// Stable slot id used for server-authoritative item sync, shared across
	// every client's copy of this same item -- filled in by whichever
	// spawned this instance (AItemSpawnPoint's authoritative index, or an
	// analogous scheme for any future spawner). See RequestPickup below for
	// why this exists: unlike GetUniqueID() (a per-process runtime id that
	// means nothing on another client), this is what lets the server's
	// first-claim-wins arbitration and every other client's copy agree on
	// which ADropItem instance just got picked up.
	// 0 is a sentinel for "never assigned one" (e.g. AEnemyBase::SpawnLoot's
	// death drops, whose contents aren't rolled through the server at all
	// yet) -- RequestPickup falls back to an immediate local pickup for
	// those rather than arbitrating on an id every untagged drop shares.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Net")
	int32 NetSlotId = 0;

	// Requests this item be picked up into TargetInventory, crediting the
	// pickup animation (if any) to PickupAnimPlayer. Both
	// OnInteract_Implementation (the local player) and
	// UCompanionAIComponent::TryPickupItem (the companion) go through this
	// single entry point instead of adding to their inventory immediately:
	// two clients could be interacting with the same synced ground item
	// (spawned from the same S2C_ItemSpawnState/S2C_ContainerLootState) at
	// the same instant, and adding it locally before the server has a
	// chance to arbitrate would duplicate it into both inventories. See
	// this function's .cpp comment for the full flow, and
	// UProtoNetClientSubsystem::OnItemPickupResult's comment for why a
	// Denied result is never acted on directly.
	//
	// When not connected/multiplayer-visible (no one else to desync with),
	// falls back to the old immediate local pickup -- a server round trip
	// that will never arrive would otherwise mean the item can never be
	// picked up at all in that mode.
	void RequestPickup(UInventoryGridComponent* TargetInventory, AProtoCharacter* PickupAnimPlayer = nullptr);

	virtual void OnInteract_Implementation(AProtoCharacter* InPlayer) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AProtoCharacter* InPlayer) const override;

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	// Fired once the server answers THIS item's pickup request (see
	// RequestPickup) -- filters OnItemPickupResult by NetSlotId, since every
	// ADropItem in the level shares that one broadcast delegate.
	UFUNCTION()
	void HandlePickupResult(int32 ResolvedNetSlotId, int32 PickerPlayerId);

	void ResolvePickup(UInventoryGridComponent* TargetInventory, AProtoCharacter* PickupAnimPlayer, bool bGrantedToMe);

	// Set once RequestPickup has actually sent a request, so a second
	// interact/AI tick before the server answers can't fire a duplicate
	// C2S_InteractRequest for the same NetSlotId.
	bool bPickupRequested = false;

	UPROPERTY()
	TWeakObjectPtr<UInventoryGridComponent> PendingTargetInventory;

	UPROPERTY()
	TWeakObjectPtr<AProtoCharacter> PendingPickupAnimPlayer;

public:
	virtual void Tick(float DeltaTime) override;
};
