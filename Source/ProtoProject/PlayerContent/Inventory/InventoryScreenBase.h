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

	// 크로스 그리드 드롭 후 소스/타겟 그리드를 재구성
	virtual void RefreshGrid(UInventoryGridComponent* Component) {}
};
