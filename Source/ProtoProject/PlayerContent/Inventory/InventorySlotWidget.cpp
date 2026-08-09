#include "InventorySlotWidget.h"
#include "ItemDragDropOperation.h"
#include "InventoryGridComponent.h"
#include "EquipmentComponent.h"
#include "Components/Border.h"

void UInventorySlotWidget::InitSlot(UInventoryScreenBase* InParentScreen, FIntPoint InSlotPosition, UInventoryGridComponent* InOwningComponent)
{
	ParentScreen = InParentScreen;
	SlotPosition = InSlotPosition;
	OwningInventoryComponent = InOwningComponent;

	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
}

void UInventorySlotWidget::SetHighlight(bool bShowHighlight, bool bIsValid)
{
	if (!SlotBorder) return;
	
	if (bShowHighlight)
	{
		SlotBorder->SetBrushColor(bIsValid ? ValidColor : InvalidColor);
	}
	else
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
}

bool UInventorySlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && ParentScreen)
	{
		FIntPoint TargetTopLeft = SlotPosition - DragOp->DragOffset;
		bool bCrossGrid = DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != OwningInventoryComponent;
		int32 IgnoreIdx = bCrossGrid ? INDEX_NONE : DragOp->ItemIndex;
		ParentScreen->UpdateDragHighlight(TargetTopLeft, DragOp->DraggedItemData, DragOp->bCurrentRotated, IgnoreIdx, OwningInventoryComponent);
		return true;
	}
	return false;
}

void UInventorySlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (ParentScreen)
	{
		ParentScreen->ClearDragHighlight();
	}
}

bool UInventorySlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && ParentScreen)
	{
		ParentScreen->ClearDragHighlight();

		FIntPoint TargetTopLeft = SlotPosition - DragOp->DragOffset;

		if (DragOp->SourceEquipmentComponent)
		{
			// 장착 해제 (드래그 위치 지정)
			if (DragOp->SourceEquipmentComponent->UnequipToInventoryAt(OwningInventoryComponent, DragOp->SourceEquipmentSlot, TargetTopLeft, DragOp->bCurrentRotated))
			{
				ParentScreen->RefreshGrid(OwningInventoryComponent);
			}
			ParentScreen->RefreshEquipmentSlots();
			return true;
		}

		if (DragOp->SourceQuickSlotComponent)
		{
			// 퀵슬롯 해제 (드래그 위치 지정)
			if (DragOp->SourceQuickSlotComponent->UnregisterToInventoryAt(DragOp->SourceQuickSlotIndex, OwningInventoryComponent, TargetTopLeft, DragOp->bCurrentRotated))
			{
				ParentScreen->RefreshGrid(OwningInventoryComponent);
			}
			ParentScreen->RefreshQuickSlots();
			return true;
		}

		bool bCrossGrid = DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != OwningInventoryComponent;

		// 그리드 칸 드롭 처리 (항상 true 반환, 실패 시 원본 갱신)
		if (bCrossGrid)
		{
			if (!ParentScreen->OnItemDroppedFromExternal(DragOp, TargetTopLeft, DragOp->bCurrentRotated, OwningInventoryComponent))
			{
				if (DragOp->SourceScreenWidget)
				{
					DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
				}
			}
			return true;
		}

		if (!ParentScreen->OnItemDropped(DragOp->ItemIndex, TargetTopLeft, DragOp->bCurrentRotated, OwningInventoryComponent))
		{
			ParentScreen->RefreshGrid(OwningInventoryComponent);
		}
		return true;
	}
	return false;
}