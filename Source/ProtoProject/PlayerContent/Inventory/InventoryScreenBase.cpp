#include "InventoryScreenBase.h"
#include "InventorySlotWidget.h"
#include "InventoryItemWidget.h"
#include "InventoryGridComponent.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"

void UInventoryScreenBase::PopulateGridPanel(UGridPanel* GridPanel, UInventoryGridComponent* Comp,
	TSubclassOf<UInventorySlotWidget> SlotClass, TSubclassOf<UInventoryItemWidget> ItemClass,
	TMap<FIntPoint, UInventorySlotWidget*>& OutSlotMap, TArray<UInventoryItemWidget*>& OutItemWidgets)
{
	if (!GridPanel || !Comp || !SlotClass) return;

	GridPanel->ClearChildren();
	OutSlotMap.Empty();
	OutItemWidgets.Empty();

	for (int32 y = 0; y < Comp->GridRows; ++y)
	{
		for (int32 x = 0; x < Comp->GridColumns; ++x)
		{
			UInventorySlotWidget* NewSlot = CreateWidget<UInventorySlotWidget>(GetOwningPlayer(), SlotClass);
			if (!NewSlot) continue;

			NewSlot->InitSlot(this, FIntPoint(x, y), Comp);
			OutSlotMap.Add(FIntPoint(x, y), NewSlot);

			if (UGridSlot* GridSlot = GridPanel->AddChildToGrid(NewSlot))
			{
				GridSlot->SetRow(y);
				GridSlot->SetColumn(x);
				GridSlot->SetPadding(FMargin(1.2f));
				GridSlot->SetHorizontalAlignment(HAlign_Fill);
				GridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}

	if (!ItemClass) return;

	for (int32 i = 0; i < Comp->Items.Num(); ++i)
	{
		const FInventoryItemInstance& Item = Comp->Items[i];
		UInventoryItemWidget* ItemWidget = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), ItemClass);
		if (!ItemWidget) continue;

		ItemWidget->InitItem(this, Comp, i);
		OutItemWidgets.Add(ItemWidget);

		if (UGridSlot* ItemGridSlot = GridPanel->AddChildToGrid(ItemWidget))
		{
			ItemGridSlot->SetColumn(Item.GridPosition.X);
			ItemGridSlot->SetRow(Item.GridPosition.Y);
			FIntPoint Size = Item.GetEffectiveSize();
			ItemGridSlot->SetColumnSpan(Size.X);
			ItemGridSlot->SetRowSpan(Size.Y);
			ItemGridSlot->SetPadding(FMargin(1.0f));
			ItemGridSlot->SetLayer(1);
			ItemGridSlot->SetHorizontalAlignment(HAlign_Fill);
			ItemGridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
}

void UInventoryScreenBase::RefreshItemWidgetInMap(int32 ItemIndex, UInventoryGridComponent* Comp, const TArray<UInventoryItemWidget*>& ItemWidgets)
{
	if (!Comp || !Comp->Items.IsValidIndex(ItemIndex) || !ItemWidgets.IsValidIndex(ItemIndex)) return;
	UInventoryItemWidget* Widget = ItemWidgets[ItemIndex];
	if (!Widget) return;
	UGridSlot* GridSlot = Cast<UGridSlot>(Widget->Slot);
	if (!GridSlot) return;

	const FInventoryItemInstance& Item = Comp->Items[ItemIndex];
	FIntPoint Size = Item.GetEffectiveSize();

	GridSlot->SetColumn(Item.GridPosition.X);
	GridSlot->SetRow(Item.GridPosition.Y);
	GridSlot->SetColumnSpan(Size.X);
	GridSlot->SetRowSpan(Size.Y);

	Widget->SetVisibility(ESlateVisibility::Visible);
	Widget->RefreshVisual();
}

bool UInventoryScreenBase::TryLightRefresh(UInventoryGridComponent* Comp, int32 ItemIndex, const FGuid& ExpectedInstanceId, const TArray<UInventoryItemWidget*>& ItemWidgets)
{
	if (!Comp || !Comp->Items.IsValidIndex(ItemIndex)) return false;
	if (Comp->Items[ItemIndex].InstanceId != ExpectedInstanceId) return false;

	RefreshItemWidgetInMap(ItemIndex, Comp, ItemWidgets);
	return true;
}

void UInventoryScreenBase::ApplyDragHighlightToMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap, const FIntPoint& TargetTopLeft, const FIntPoint& Size, bool bIsValid)
{
	for (int32 x = 0; x < Size.X; ++x)
	{
		for (int32 y = 0; y < Size.Y; ++y)
		{
			FIntPoint CheckPos(TargetTopLeft.X + x, TargetTopLeft.Y + y);
			if (UInventorySlotWidget** Found = SlotMap.Find(CheckPos))
			{
				(*Found)->SetHighlight(true, bIsValid);
			}
		}
	}
}

void UInventoryScreenBase::ClearHighlightMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap)
{
	for (auto& Pair : SlotMap)
	{
		Pair.Value->SetHighlight(false, false);
	}
}

bool UInventoryScreenBase::HandleRotateDragKey()
{
	if (!ActiveDragOp || !ActiveDragOp->DraggedItemData) return false;

	const bool bNewRotated = !ActiveDragOp->bCurrentRotated;
	ActiveDragOp->bCurrentRotated = bNewRotated;

	const FIntPoint NewSize = bNewRotated
		? FIntPoint(ActiveDragOp->DraggedItemData->GridHeight, ActiveDragOp->DraggedItemData->GridWidth)
		: FIntPoint(ActiveDragOp->DraggedItemData->GridWidth, ActiveDragOp->DraggedItemData->GridHeight);

	if (ActiveDragOp->DragVisualMatInst)
	{
		ActiveDragOp->DragVisualMatInst->SetScalarParameterValue(FName("rotation"), bNewRotated ? -0.25f : 0.f);
	}

	if (ActiveDragOp->DragVisualWrapper)
	{
		ActiveDragOp->DragVisualWrapper->SetWidthOverride(NewSize.X * ActiveDragOp->CellPixelSize.X);
		ActiveDragOp->DragVisualWrapper->SetHeightOverride(NewSize.Y * ActiveDragOp->CellPixelSize.Y);
	}

	ActiveDragOp->DragOffset.X = FMath::Clamp(ActiveDragOp->DragOffset.X, 0, NewSize.X - 1);
	ActiveDragOp->DragOffset.Y = FMath::Clamp(ActiveDragOp->DragOffset.Y, 0, NewSize.Y - 1);

	return true;
}
