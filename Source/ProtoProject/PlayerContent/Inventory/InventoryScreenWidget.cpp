#include "InventoryScreenWidget.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "InventoryGridComponent.h"
#include "ItemDragDropOperation.h"
#include "InputCoreTypes.h"

void UInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}


void UInventoryScreenWidget::OnItemHoverBegin(int32 ItemIndex)
{
	HoveredItemIndex = ItemIndex;
}

void UInventoryScreenWidget::OnItemHoverEnd(int32 ItemIndex)
{
	if (HoveredItemIndex == ItemIndex)
	{
		HoveredItemIndex = INDEX_NONE;
	}
}

bool UInventoryScreenWidget::OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated)
{
	if (!CachedInventoryComponent) return false;

	if (CachedInventoryComponent->PlaceItem(ItemIndex, TargetPosition, bDropRotated))
	{
		RefreshItemWidget(ItemIndex);
		ActiveDragOp = nullptr;
		return true;
	}
	return false;
}

bool UInventoryScreenWidget::OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated)
{
	if (!CachedInventoryComponent || !DragOp || !DragOp->SourceInventoryComponent) return false;

	UItemDataBase* ItemData = DragOp->DraggedItemData;
	if (!ItemData) return false;

	FIntPoint ItemSize = bDropRotated
		? FIntPoint(ItemData->GridHeight, ItemData->GridWidth)
		: FIntPoint(ItemData->GridWidth,  ItemData->GridHeight);

	if (!CachedInventoryComponent->CanPlaceAt(TargetPosition, ItemSize)) return false;

	DragOp->SourceInventoryComponent->RemoveItemAt(DragOp->ItemIndex);
	CachedInventoryComponent->AddItemAt(ItemData, TargetPosition, bDropRotated);

	if (DragOp->SourceScreenWidget)
	{
		DragOp->SourceScreenWidget->InitializeGrid(DragOp->SourceInventoryComponent);
	}
	InitializeGrid(CachedInventoryComponent);

	ActiveDragOp = nullptr;
	return true;
}

void UInventoryScreenWidget::SetActiveDragOperation(UItemDragDropOperation* InDragOp)
{
	ActiveDragOp = InDragOp;
}

FReply UInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		if (ActiveDragOp && ActiveDragOp->DraggedItemData)
		{
			// 드래그 중 회전
			bool bNewRotated = !ActiveDragOp->bCurrentRotated;
			ActiveDragOp->bCurrentRotated = bNewRotated;

			FIntPoint NewSize = bNewRotated
				? FIntPoint(ActiveDragOp->DraggedItemData->GridHeight, ActiveDragOp->DraggedItemData->GridWidth)
				: FIntPoint(ActiveDragOp->DraggedItemData->GridWidth,  ActiveDragOp->DraggedItemData->GridHeight);

			if (ActiveDragOp->DragVisualMatInst)
			{
				ActiveDragOp->DragVisualMatInst->SetScalarParameterValue(FName("rotation"), bNewRotated ? -0.25f : 0.f);
			}

			if (ActiveDragOp->DragVisualWrapper)
			{
				ActiveDragOp->DragVisualWrapper->SetWidthOverride(NewSize.X * ActiveDragOp->CellPixelSize.X);
				ActiveDragOp->DragVisualWrapper->SetHeightOverride(NewSize.Y * ActiveDragOp->CellPixelSize.Y);
			}

			// DragOffset이 새 크기 범위를 벗어나지 않도록 클램프
			ActiveDragOp->DragOffset.X = FMath::Clamp(ActiveDragOp->DragOffset.X, 0, NewSize.X - 1);
			ActiveDragOp->DragOffset.Y = FMath::Clamp(ActiveDragOp->DragOffset.Y, 0, NewSize.Y - 1);

			return FReply::Handled();
		}

		if (HoveredItemIndex != INDEX_NONE)
		{
			if (CachedInventoryComponent && CachedInventoryComponent->RotateItem(HoveredItemIndex))
			{
				RefreshItemWidget(HoveredItemIndex);
			}
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInventoryScreenWidget::RefreshItemWidget(int32 ItemIndex)
{
	if (!CachedInventoryComponent || !ItemWidgets.IsValidIndex(ItemIndex)) return;
	UInventoryItemWidget* Widget = ItemWidgets[ItemIndex];
	if (!Widget) return;
	UGridSlot* GridSlot = Cast<UGridSlot>(Widget->Slot);
	if (!GridSlot) return;

	const FInventoryItemInstance& Item = CachedInventoryComponent->Items[ItemIndex];
	FIntPoint Size = Item.GetEffectiveSize();

	GridSlot->SetColumn(Item.GridPosition.X);
	GridSlot->SetRow(Item.GridPosition.Y);
	GridSlot->SetColumnSpan(Size.X);
	GridSlot->SetRowSpan(Size.Y);

	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->RefreshVisual();
}

void UInventoryScreenWidget::InitializeGrid(UInventoryGridComponent* InInventoryComponent)
{
	if (!InInventoryComponent || !InventoryGridPanel || !SlotWidgetClass)
	{
		return;
	}

	CachedInventoryComponent = InInventoryComponent;
	HoveredItemIndex = INDEX_NONE;
	ItemWidgets.Empty();
	SlotWidgetMap.Empty(); 
	InventoryGridPanel->ClearChildren();

	int32 Rows = InInventoryComponent->GridRows;
	int32 Columns = InInventoryComponent->GridColumns;

	for (int32 y = 0; y < Rows; ++y)
	{
		for (int32 x = 0; x < Columns; ++x)
		{
			UInventorySlotWidget* NewSlotWidget = CreateWidget<UInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
			if (NewSlotWidget)
			{
				NewSlotWidget->InitSlot(this, FIntPoint(x, y));
				
				// 생성된 슬롯을 Map에 기억해 둡니다.
				SlotWidgetMap.Add(FIntPoint(x, y), NewSlotWidget);

				UGridSlot* GridSlot = InventoryGridPanel->AddChildToGrid(NewSlotWidget);
				if (GridSlot)
				{
					GridSlot->SetRow(y);
					GridSlot->SetColumn(x);
					GridSlot->SetPadding(FMargin(1.2f));
					GridSlot->SetHorizontalAlignment(HAlign_Fill);
					GridSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
	}

	if (!ItemWidgetClass)
	{
		return;
	}

	for (int32 i = 0; i < InInventoryComponent->Items.Num(); ++i)
	{
		const FInventoryItemInstance& Item = InInventoryComponent->Items[i];

		UInventoryItemWidget* NewItemWidget = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), ItemWidgetClass);
		if (NewItemWidget)
		{
			NewItemWidget->InitItem(this, InInventoryComponent, i);
			ItemWidgets.Add(NewItemWidget);

			UGridSlot* ItemGridSlot = InventoryGridPanel->AddChildToGrid(NewItemWidget);
			if (ItemGridSlot)
			{
				ItemGridSlot->SetColumn(Item.GridPosition.X);
				ItemGridSlot->SetRow(Item.GridPosition.Y);

				FIntPoint ItemSize = Item.GetEffectiveSize();
				ItemGridSlot->SetColumnSpan(ItemSize.X);
				ItemGridSlot->SetRowSpan(ItemSize.Y);

				ItemGridSlot->SetPadding(FMargin(1.0f));
				ItemGridSlot->SetLayer(1);
				ItemGridSlot->SetHorizontalAlignment(HAlign_Fill);
				ItemGridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
}
void UInventoryScreenWidget::UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex)
{
	ClearDragHighlight();
	if (!CachedInventoryComponent || !ItemData) return;

	FIntPoint Size = bRotated ? FIntPoint(ItemData->GridHeight, ItemData->GridWidth) : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
	bool bIsValid = CachedInventoryComponent->CanPlaceAt(TargetTopLeft, Size, IgnoreIndex);

	for (int x = 0; x < Size.X; ++x)
	{
		for (int y = 0; y < Size.Y; ++y)
		{
			FIntPoint CheckPos(TargetTopLeft.X + x, TargetTopLeft.Y + y);
			if (UInventorySlotWidget** FoundSlot = SlotWidgetMap.Find(CheckPos))
			{
				(*FoundSlot)->SetHighlight(true, bIsValid);
			}
		}
	}
}
// 하이라이트를 지웁니다.
void UInventoryScreenWidget::ClearDragHighlight()
{
	for (auto& Pair : SlotWidgetMap)
	{
		Pair.Value->SetHighlight(false, false);
	}
}
