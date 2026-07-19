#include "InventorySlotWidget.h"
#include "InventoryScreenWidget.h"
#include "ItemDragDropOperation.h"
#include "InventoryGridComponent.h"
#include "Components/Border.h"

void UInventorySlotWidget::InitSlot(UInventoryScreenWidget* InParentScreen, FIntPoint InSlotPosition)
{
	ParentScreen = InParentScreen;
	SlotPosition = InSlotPosition;

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
		bool bCrossGrid = DragOp->SourceInventoryComponent
			&& DragOp->SourceInventoryComponent != ParentScreen->GetCachedInventoryComponent();
		int32 IgnoreIdx = bCrossGrid ? INDEX_NONE : DragOp->ItemIndex;
		ParentScreen->UpdateDragHighlight(TargetTopLeft, DragOp->DraggedItemData, DragOp->bCurrentRotated, IgnoreIdx);
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
		bool bCrossGrid = DragOp->SourceInventoryComponent
			&& DragOp->SourceInventoryComponent != ParentScreen->GetCachedInventoryComponent();

		if (bCrossGrid)
		{
			return ParentScreen->OnItemDroppedFromExternal(DragOp, TargetTopLeft, DragOp->bCurrentRotated);
		}
		return ParentScreen->OnItemDropped(DragOp->ItemIndex, TargetTopLeft, DragOp->bCurrentRotated);
	}
	return false;
}