#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EquipmentComponent.generated.h"

class UItemDataBase;
class UInventoryGridComponent;

UENUM(BlueprintType)
enum class EEquipmentSlot : uint8
{
    Helmet,
    Vest,
    Weapon1,
    Weapon2
};

USTRUCT(BlueprintType)
struct FEquippedItem
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    UItemDataBase* ItemData = nullptr;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
    FGuid SourceInstanceId;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEquipmentChanged, EEquipmentSlot, ChangedSlot);

// 장착 슬롯(헬멧/조끼/무기1/무기2) 관리 컴포넌트
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "Equipment"))
class PROTOPROJECT_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    // 슬롯 장착 가능 여부 검사
    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool CanEquipToSlot(UItemDataBase* ItemData, EEquipmentSlot Slot) const;

    // 자동 장착 대상 슬롯 결정
    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool ResolveTargetSlot(UItemDataBase* ItemData, EEquipmentSlot& OutSlot) const;

    // 인벤토리 → 장착
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipFromInventory(UInventoryGridComponent* SourceInventory, const FGuid& InstanceId, EEquipmentSlot Slot);

    // 장착 → 인벤토리 해제 (자동 배치)
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipToInventory(UInventoryGridComponent* TargetInventory, EEquipmentSlot Slot);

    // 장착 → 인벤토리 해제 (드래그 위치 지정)
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipToInventoryAt(UInventoryGridComponent* TargetInventory, EEquipmentSlot Slot, const FIntPoint& Position, bool bRotated);

    // 장착 슬롯 스왑/이동
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool SwapOrMoveSlots(EEquipmentSlot SourceSlot, EEquipmentSlot TargetSlot);

    UFUNCTION(BlueprintPure, Category = "Equipment")
    const FEquippedItem& GetEquippedItem(EEquipmentSlot Slot) const;

    // 모든 장착 슬롯을 비운다. 내용물은 인벤토리로 반환하지 않고 소실된다(사망 페널티용).
    // 채워져 있던 슬롯마다 OnEquipmentChanged를 브로드캐스트한다.
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    void ClearAll();

    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentChanged OnEquipmentChanged;

private:
    UPROPERTY(VisibleAnywhere, Category = "Equipment")
    TArray<FEquippedItem> EquippedSlots;
};
