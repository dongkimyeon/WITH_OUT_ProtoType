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

// 인벤토리 그리드 좌표계와 무관하게, 카테고리/서브타입 매칭만으로 헬멧/조끼/무기1/무기2를 장착하는 컴포넌트.
// 실제 캐릭터 메시에 무기를 부착하는 기존 AWeaponBase 시스템과는 별개의 상태로 동작한다 (연동은 후속 작업).
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent, PrioritizeCategories = "Equipment"))
class PROTOPROJECT_API UEquipmentComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    // ItemData가 Slot에 장착 가능한 카테고리/서브타입인지 검사 (그리드 크기는 검사하지 않음)
    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool CanEquipToSlot(UItemDataBase* ItemData, EEquipmentSlot Slot) const;

    // 우클릭 자동 장착 시 사용 - 아이템 타입에 맞는 슬롯을 자동으로 결정
    UFUNCTION(BlueprintPure, Category = "Equipment")
    bool ResolveTargetSlot(UItemDataBase* ItemData, EEquipmentSlot& OutSlot) const;

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool EquipFromInventory(UInventoryGridComponent* SourceInventory, const FGuid& InstanceId, EEquipmentSlot Slot);

    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool UnequipToInventory(UInventoryGridComponent* TargetInventory, EEquipmentSlot Slot);

    // 슬롯 간 드래그(예: 무기1 <-> 무기2)로 장착된 아이템끼리 자리를 바꾸거나 빈 슬롯으로 옮긴다.
    UFUNCTION(BlueprintCallable, Category = "Equipment")
    bool SwapOrMoveSlots(EEquipmentSlot SourceSlot, EEquipmentSlot TargetSlot);

    UFUNCTION(BlueprintPure, Category = "Equipment")
    const FEquippedItem& GetEquippedItem(EEquipmentSlot Slot) const;

    UPROPERTY(BlueprintAssignable, Category = "Equipment")
    FOnEquipmentChanged OnEquipmentChanged;

private:
    UPROPERTY(VisibleAnywhere, Category = "Equipment")
    TArray<FEquippedItem> EquippedSlots;
};
