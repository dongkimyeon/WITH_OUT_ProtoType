#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "QuickSlotComponent.generated.h"

class UItemDataBase;
class UInventoryGridComponent;
class AProtoCharacter;

USTRUCT(BlueprintType)
struct FQuickSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	UItemDataBase* ItemData = nullptr;

	// 0 = 빈 슬롯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "QuickSlot")
	int32 StackCount = 0;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuickSlotChanged, int32, SlotIndex);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "QuickSlot"))
class PROTOPROJECT_API UQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuickSlotComponent();

	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot")
	int32 NumSlots = 8;

	// ItemData가 퀵슬롯에 등록 가능한지 검사
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	bool CanRegisterToQuickSlot(UItemDataBase* ItemData) const;

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool RegisterFromInventory(int32 SlotIndex, UInventoryGridComponent* SourceInventory, const FGuid& InstanceId);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UnregisterToInventory(int32 SlotIndex, UInventoryGridComponent* TargetInventory);

	// 우클릭 해제(자동 배치)와 달리, 드래그로 놓은 정확한 위치/회전 상태에 스택 전체를 한 번에 배치한다.
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UnregisterToInventoryAt(int32 SlotIndex, UInventoryGridComponent* TargetInventory, const FIntPoint& Position, bool bRotated);

	// 퀵슬롯 칸끼리 드래그로 자리를 바꾸거나 빈 슬롯으로 옮긴다.
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool SwapOrMoveSlots(int32 SourceIndex, int32 TargetIndex);

	// 소비 효과를 적용하고 수량을 1 소모한다. 0이 되면 슬롯을 비운다.
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UseQuickSlot(int32 SlotIndex, AProtoCharacter* OwningCharacter);

	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	const FQuickSlotEntry& GetQuickSlotEntry(int32 SlotIndex) const;

	// 4번 키를 짧게 탭했을 때 재사용할 슬롯 (마지막으로 Radial에서 선택/사용한 슬롯)
	UPROPERTY(BlueprintReadWrite, Category = "QuickSlot")
	int32 LastUsedSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintAssignable, Category = "QuickSlot")
	FOnQuickSlotChanged OnQuickSlotChanged;

protected:
	virtual void BeginPlay() override;

private:
	// LastUsedSlotIndex가 비어있거나(INDEX_NONE) 방금 비워진 슬롯을 가리키게 됐을 때,
	// 점유된 슬롯 중 가장 앞의 것으로 자동 대체한다. 이미 유효한 슬롯을 가리키고 있으면 건드리지 않는다
	// (등록/해제/스왑으로 다른 칸이 바뀌었다고 해서 플레이어가 고른 슬롯이 임의로 바뀌면 안 되기 때문).
	void EnsureValidLastUsedSlot();

	UPROPERTY(VisibleAnywhere, Category = "QuickSlot")
	TArray<FQuickSlotEntry> Slots;
};
