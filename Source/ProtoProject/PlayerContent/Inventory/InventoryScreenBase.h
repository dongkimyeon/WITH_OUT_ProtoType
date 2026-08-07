#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenBase.generated.h"

class UItemDataBase;
class UInventoryGridComponent;
class UItemDragDropOperation;

UCLASS(Abstract)
class PROTOPROJECT_API UInventoryScreenBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual FVector2D GetCellPixelSize() const { return FVector2D(75.f, 75.f); }

	virtual void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent) {}
	virtual void ClearDragHighlight() {}

	virtual bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) { return false; }
	virtual bool OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) { return false; }

	virtual void SetActiveDragOperation(UItemDragDropOperation* InDragOp) {}
	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 아이템 우클릭 시 호출됨 - 아이템 데이터의 GetContextAction()에 따라 장착/사용 등으로 분기
	virtual void OnItemContextAction(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 아이템 Ctrl+우클릭 시 호출됨 - 스택 아이템이면 수량을 지정해서 버리는 팝업을 띄운다.
	virtual void OnItemRequestPartialDrop(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 장착 슬롯 UI를 다시 그린다 (장착/해제/스왑 이후 호출)
	virtual void RefreshEquipmentSlots() {}

	// 퀵슬롯 등록 UI를 다시 그린다 (등록/해제/스왑 이후 호출)
	virtual void RefreshQuickSlots() {}

	virtual void RefreshGrid(UInventoryGridComponent* Component) {}
};
