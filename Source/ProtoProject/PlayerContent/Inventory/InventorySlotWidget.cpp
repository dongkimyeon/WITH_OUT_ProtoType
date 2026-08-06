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

		if (DragOp->SourceEquipmentComponent)
		{
			// 성공/실패와 무관하게 항상 갱신해야, 드래그 시작 시 숨겨졌던 원본 장착 슬롯 아이콘이 복원된다.
			if (DragOp->SourceEquipmentComponent->UnequipToInventory(OwningInventoryComponent, DragOp->SourceEquipmentSlot))
			{
				ParentScreen->RefreshGrid(OwningInventoryComponent);
			}
			ParentScreen->RefreshEquipmentSlots();
			return true;
		}

		FIntPoint TargetTopLeft = SlotPosition - DragOp->DragOffset;
		bool bCrossGrid = DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != OwningInventoryComponent;

		if (bCrossGrid)
		{
			return ParentScreen->OnItemDroppedFromExternal(DragOp, TargetTopLeft, DragOp->bCurrentRotated, OwningInventoryComponent);
		}
		return ParentScreen->OnItemDropped(DragOp->ItemIndex, TargetTopLeft, DragOp->bCurrentRotated, OwningInventoryComponent);
	}
	return false;
}