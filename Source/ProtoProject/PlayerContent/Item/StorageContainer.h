#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "../Interactable.h"
#include "StorageContainer.generated.h"

class UStaticMeshComponent;
class UInventoryGridComponent;
class UItemDataBase;
class AProtoCharacter;

UCLASS(meta = (PrioritizeCategories = "Container"))
class PROTOPROJECT_API AStorageContainer : public AActor, public IInteractable
{
	GENERATED_BODY()

public:
	AStorageContainer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UBoxComponent* InteractBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInventoryGridComponent* ContainerInventory;

	UPROPERTY(EditAnywhere, Category = "Container")
	TArray<TObjectPtr<UItemDataBase>> DefaultLootItems;

	UPROPERTY(EditAnywhere, Category = "Container")
	FText ContainerName;

	virtual void OnInteract_Implementation(AProtoCharacter* InPlayer) override;
	virtual FText GetInteractPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(AProtoCharacter* InPlayer) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnInteractBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnInteractEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);
};
