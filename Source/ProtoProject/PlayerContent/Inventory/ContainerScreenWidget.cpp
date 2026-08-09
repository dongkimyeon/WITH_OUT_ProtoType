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
		PopulateGridPanel(PlayerGridPanel, PlayerComp, SlotWidgetClass, ItemWidgetClass, PlayerSlotMap, PlayerItemWidgets);
	if (ContainerGridPanel && ContainerComp)
		PopulateGridPanel(ContainerGridPanel, ContainerComp, SlotWidgetClass, ItemWidgetClass, ContainerSlotMap, ContainerItemWidgets);
}

void UContainerScreenWidget::RefreshGrid(UInventoryGridComponent* Component)
{
	if (Component == PlayerInventoryComponent)
		PopulateGridPanel(PlayerGridPanel, Component, SlotWidgetClass, ItemWidgetClass, PlayerSlotMap, PlayerItemWidgets);
	else if (Component == ContainerInventoryComponent)
		PopulateGridPanel(ContainerGridPanel, Component, SlotWidgetClass, ItemWidgetClass, ContainerSlotMap, ContainerItemWidgets);
}

void UContainerScreenWidget::RefreshSingleItem(UInventoryGridComponent* Component, int32 ItemIndex, const FGuid& ExpectedInstanceId)
{
	TArray<UInventoryItemWidget*>* TargetItemWidgets = nullptr;
	if (Component == PlayerInventoryComponent) TargetItemWidgets = &PlayerItemWidgets;
	else if (Component == ContainerInventoryComponent) TargetItemWidgets = &ContainerItemWidgets;
	if (!TargetItemWidgets) return;

	if (!TryLightRefresh(Component, ItemIndex, ExpectedInstanceId, *TargetItemWidgets))
	{
		// 인덱스 밀림 시 전체 재생성
		RefreshGrid(Component);
	}
}

void UContainerScreenWidget::UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent)
{
	ClearDragHighlight();
	if (!TargetComponent || !ItemData) return;

	TMap<FIntPoint, UInventorySlotWidget*>& SlotMap = (TargetComponent == PlayerInventoryComponent) ? PlayerSlotMap : ContainerSlotMap;
	FIntPoint Size = bRotated ? FIntPoint(ItemData->GridHeight, ItemData->GridWidth) : FIntPoint(ItemData->GridWidth, ItemData->GridHeight);
	bool bIsValid = TargetComponent->CanPlaceAt(TargetTopLeft, Size, IgnoreIndex);

	ApplyDragHighlightToMap(SlotMap, TargetTopLeft, Size, bIsValid);
}

void UContainerScreenWidget::ClearDragHighlight()
{
	ClearHighlightMap(PlayerSlotMap);
	ClearHighlightMap(ContainerSlotMap);
}

bool UContainerScreenWidget::OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent)
{
	if (!TargetComponent || !TargetComponent->PlaceItem(ItemIndex, TargetPosition, bDropRotated)) return false;

	TArray<UInventoryItemWidget*>& ItemWidgets = (TargetComponent == PlayerInventoryComponent) ? PlayerItemWidgets : ContainerItemWidgets;
	RefreshItemWidgetInMap(ItemIndex, TargetComponent, ItemWidgets);

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

	FInventoryItemInstance SourceInstance;
	const int32 StackCount = DragOp->SourceInventoryComponent->FindInstanceById(DragOp->InstanceId, SourceInstance) ? SourceInstance.StackCount : 1;

	DragOp->SourceInventoryComponent->RemoveInstanceById(DragOp->InstanceId);
	TargetComponent->AddItemAt(ItemData, TargetPosition, bDropRotated, StackCount);

	if (DragOp->SourceScreenWidget)
		DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
	RefreshGrid(TargetComponent);

	ActiveDragOp = nullptr;
	return true;
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
	if (InKeyEvent.GetKey() == EKeys::R && HandleRotateDragKey())
	{
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
