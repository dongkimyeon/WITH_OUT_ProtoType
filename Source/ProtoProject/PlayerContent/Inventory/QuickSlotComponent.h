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

// 인벤토리 화면의 8칸 등록 UI를 통해 소비 아이템을 미리 등록해두고, 게임플레이 중 4번 키(꾹 눌러 Radial 선택,
// 짧게 탭해 마지막 슬롯 재사용)로 사용하는 퀵슬롯. 장착(EquipmentComponent)과 동일하게 등록 시
// 그리드에서 아이템을 물리적으로 제거해 슬롯이 소유권을 갖는다.
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "QuickSlot"))
class PROTOPROJECT_API UQuickSlotComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UQuickSlotComponent();

	UPROPERTY(EditDefaultsOnly, Category = "QuickSlot")
	int32 NumSlots = 8;

	// ItemData가 퀵슬롯에 등록 가능한지 (소비 가능한 아이템인지) 검사
	UFUNCTION(BlueprintPure, Category = "QuickSlot")
	bool CanRegisterToQuickSlot(UItemDataBase* ItemData) const;

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool RegisterFromInventory(int32 SlotIndex, UInventoryGridComponent* SourceInventory, const FGuid& InstanceId);

	UFUNCTION(BlueprintCallable, Category = "QuickSlot")
	bool UnregisterToInventory(int32 SlotIndex, UInventoryGridComponent* TargetInventory);

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
	UPROPERTY(VisibleAnywhere, Category = "QuickSlot")
	TArray<FQuickSlotEntry> Slots;
};
