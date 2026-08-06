#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BoxComponent.h"
#include "ItemDataBase.h"
#include "../Interactable.h"
#include "DropItem.generated.h"

class UStaticMeshComponent;
class AProtoCharacter;

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

	// 겹칠 수 있는(스택 가능) 아이템일 때, 이 하나의 월드 액터가 실제로 담고 있는 개수.
	// 보이는 건 하나지만 주웠을 때 이 수량만큼 인벤토리에 반영된다.
	UPROPERTY(EditAnywhere, Category = "Data")
	int32 StackCount = 1;

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

public:
	virtual void Tick(float DeltaTime) override;
};
