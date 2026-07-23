#include "ContainerScreenWidget.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "ItemDragDropOperation.h"
#include "InputCoreTypes.h"

void UContainerScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!IsDesignTime() || !SlotWidgetClass) return;

	auto FillPreviewGrid = [&](UGridPanel* GridPanel, TSubclassOf<UInventoryGridComponent> GridClass, int32 DefaultCols, int32 DefaultRows)
	{
		if (!GridPanel) return;

		int32 Cols = DefaultCols;
		int32 Rows = DefaultRows;
		if (GridClass)
		{
			const UInventoryGridComponent* CDO = GridClass->GetDefaultObject<UInventoryGridComponent>();
			Cols = CDO->GridColumns;
			Rows = CDO->GridRows;
		}

		GridPanel->ClearChildren();
		for (int32 y = 0; y < Rows; ++y)
		{
			for (int32 x = 0; x < Cols; ++x)
			{
				UInventorySlotWidget* NewSlot = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
				if (!NewSlot) continue;

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
	};

	FillPreviewGrid(PlayerGridPanel, PreviewPlayerGridClass, 10, 6);
	FillPreviewGrid(ContainerGridPanel, PreviewContainerGridClass, 5, 4);
}

void UContainerScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
}

void UContainerScreenWidget::InitializeScreen(UInventoryGridComponent* PlayerComp, UInventoryGridComponent* ContainerComp)
{
	PlayerInventoryComponent = PlayerComp;
	ContainerInventoryComponent = ContainerComp;

	if (PlayerGridPanel && PlayerComp)
		PopulateGrid(PlayerGridPanel, PlayerComp, PlayerSlotMap, PlayerItemWidgets);
	if (ContainerGridPanel && ContainerComp)
		PopulateGrid(ContainerGridPanel, ContainerComp, ContainerSlotMap, ContainerItemWidgets);
}

void UContainerScreenWidget::PopulateGrid(UGridPanel* GridPanel, UInventoryGridComponent* Comp,
	TMap<FIntPoint, UInventorySlotWidget*>& OutSlotMap, TArray<UInventoryItemWidget*>& OutItemWidgets)
{
	GridPanel->ClearChildren();
	OutSlotMap.Empty();
	OutItemWidgets.Empty();

	for (int32 y = 0; y < Comp->GridRows; ++y)
	{
		for (int32 x = 0; x < Comp->GridColumns; ++x)
		{
			UInventorySlotWidget* NewSlot = CreateWidget<UInventorySlotWidget>(GetOwningPlayer(), SlotWidgetClass);
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

	if (!ItemWidgetClass) return;

	for (int32 i = 0; i < Comp->Items.Num(); ++i)
	{
		const FInventoryItemInstance& Item = Comp->Items[i];
		UInventoryItemWidget* ItemWidget = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), ItemWidgetClass);
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

void UContainerScreenWidget::RefreshGrid(UInventoryGridComponent* Component)
{
	if (Component == PlayerInventoryComponent)
		PopulateGrid(PlayerGridPanel, Component, PlayerSlotMap, PlayerItemWidgets);
	else if (Component == ContainerInventoryComponent)
		PopulateGrid(ContainerGridPanel, Component, ContainerSlotMap, ContainerItemWidgets);
}

void UContainerScreenWidget::UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent)
{
	ClearDragHighlight();
	if (!TargetComponent || !ItemData) return;

	TMap<FIntPoint, UInventorySlotWidget*>& SlotMap = (TargetComponent == PlayerInventoryComponent) ? PlayerSlotMap : ContainerSlotMap;
	FIntPoint Size = bRotated ? FIntPoint(ItemData->GridHeight, ItemData->GridWidth) : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
	bool bIsValid = TargetComponent->CanPlaceAt(TargetTopLeft, Size, IgnoreIndex);

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

void UContainerScreenWidget::ClearDragHighlight()
{
	for (auto& Pair : PlayerSlotMap) Pair.Value->SetHighlight(false, false);
	for (auto& Pair : ContainerSlotMap) Pair.Value->SetHighlight(false, false);
}

bool UContainerScreenWidget::OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent)
{
	if (!TargetComponent || !TargetComponent->PlaceItem(ItemIndex, TargetPosition, bDropRotated)) return false;

	TArray<UInventoryItemWidget*>& ItemWidgets = (TargetComponent == PlayerInventoryComponent) ? PlayerItemWidgets : ContainerItemWidgets;
	RefreshItemWidgetInGrid(ItemIndex, TargetComponent, ItemWidgets);

	ActiveDragOp = nullptr;
	return true;
}

bool UContainerScreenWidget::OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent)
{
	if (!TargetComponent || !DragOp || !DragOp->SourceInventoryComponent) return false;

	UItemDataBase* ItemData = DragOp->DraggedItemData;
	if (!ItemData) return false;

	FIntPoint ItemSize = bDropRotated
		? FIntPoint(ItemData->GridHeight, ItemData->GridWidth)
		: FIntPoint(ItemData->GridWidth,  ItemData->GridHeight);

	if (!TargetComponent->CanPlaceAt(TargetPosition, ItemSize)) return false;

	DragOp->SourceInventoryComponent->RemoveItemAt(DragOp->ItemIndex);
	TargetComponent->AddItemAt(ItemData, TargetPosition, bDropRotated);

	if (DragOp->SourceScreenWidget)
		DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
	RefreshGrid(TargetComponent);

	ActiveDragOp = nullptr;
	return true;
}

void UContainerScreenWidget::SetActiveDragOperation(UItemDragDropOperation* InDragOp)
{
	ActiveDragOp = InDragOp;
}

void UContainerScreenWidget::OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent)
{
	if (OwningComponent == PlayerInventoryComponent) PlayerHoveredIndex = ItemIndex;
	else ContainerHoveredIndex = ItemIndex;
}

void UContainerScreenWidget::OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent)
{
	if (OwningComponent == PlayerInventoryComponent)
	{
		if (PlayerHoveredIndex == ItemIndex) PlayerHoveredIndex = INDEX_NONE;
	}
	else
	{
		if (ContainerHoveredIndex == ItemIndex) ContainerHoveredIndex = INDEX_NONE;
	}
}

FReply UContainerScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R && ActiveDragOp && ActiveDragOp->DraggedItemData)
	{
		bool bNewRotated = !ActiveDragOp->bCurrentRotated;
		ActiveDragOp->bCurrentRotated = bNewRotated;

		FIntPoint NewSize = bNewRotated
			? FIntPoint(ActiveDragOp->DraggedItemData->GridHeight, ActiveDragOp->DraggedItemData->GridWidth)
			: FIntPoint(ActiveDragOp->DraggedItemData->GridWidth,  ActiveDragOp->DraggedItemData->GridHeight);

		if (ActiveDragOp->DragVisualMatInst)
			ActiveDragOp->DragVisualMatInst->SetScalarParameterValue(FName("rotation"), bNewRotated ? -0.25f : 0.f);

		if (ActiveDragOp->DragVisualWrapper)
		{
			ActiveDragOp->DragVisualWrapper->SetWidthOverride(NewSize.X * ActiveDragOp->CellPixelSize.X);
			ActiveDragOp->DragVisualWrapper->SetHeightOverride(NewSize.Y * ActiveDragOp->CellPixelSize.Y);
		}

		ActiveDragOp->DragOffset.X = FMath::Clamp(ActiveDragOp->DragOffset.X, 0, NewSize.X - 1);
		ActiveDragOp->DragOffset.Y = FMath::Clamp(ActiveDragOp->DragOffset.Y, 0, NewSize.Y - 1);

		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UContainerScreenWidget::RefreshItemWidgetInGrid(int32 ItemIndex, UInventoryGridComponent* Comp, TArray<UInventoryItemWidget*>& ItemWidgets)
{
	if (!Comp || !ItemWidgets.IsValidIndex(ItemIndex)) return;
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
