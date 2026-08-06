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

		// 그리드 칸 안에서의 배치 성공/실패는 여기서 완전히 소비한다 (항상 true).
		// false를 반환하면 "그리드 밖으로 드롭"과 구분이 안 되어 화면 바깥 버리기 폴백으로 잘못 전파될 수 있다.
		// 실패했을 때는 관련 그리드를 다시 그려서, 드래그 시작 시 숨겨졌던 원본 아이콘을 복원한다.
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