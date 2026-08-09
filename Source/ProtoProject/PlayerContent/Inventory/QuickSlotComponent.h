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

	// 등록 가능 여부 검사
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	bool CanRegisterToQuickSlot(UItemDataBase* ItemData) const;

	// 인벤토리 → 퀵슬롯 등록
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool RegisterFromInventory(int32 SlotIndex, UInventoryGridComponent* SourceInventory, const FGuid& InstanceId);

	// 퀵슬롯 → 인벤토리 해제 (자동 배치)
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UnregisterToInventory(int32 SlotIndex, UInventoryGridComponent* TargetInventory);

	// 퀵슬롯 → 인벤토리 해제 (드래그 위치 지정)
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UnregisterToInventoryAt(int32 SlotIndex, UInventoryGridComponent* TargetInventory, const FIntPoint& Position, bool bRotated);

	// 퀵슬롯 칸 스왑/이동
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool SwapOrMoveSlots(int32 SourceIndex, int32 TargetIndex);

	// 퀵슬롯 사용 (소비 적용 + 수량 차감)
	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UseQuickSlot(int32 SlotIndex, AProtoCharacter* OwningCharacter);

	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	const FQuickSlotEntry& GetQuickSlotEntry(int32 SlotIndex) const;

	// 마지막 사용 슬롯 (짧게 탭 시 재사용)
	UPROPERTY(BlueprintReadWrite, Category = "QuickSlot")
	int32 LastUsedSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintAssignable, Category = "QuickSlot")
	FOnQuickSlotChanged OnQuickSlotChanged;

protected:
	virtual void BeginPlay() override;

private:
	// 마지막 사용 슬롯 유효성 보정
	void EnsureValidLastUsedSlot();

	UPROPERTY(VisibleAnywhere, Category = "QuickSlot")
	TArray<FQuickSlotEntry> Slots;
};
