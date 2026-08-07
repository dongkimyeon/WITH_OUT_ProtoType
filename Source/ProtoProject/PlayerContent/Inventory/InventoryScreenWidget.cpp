#include "InventoryScreenWidget.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "ItemDragDropOperation.h"
#include "InputCoreTypes.h"
#include "EquipmentComponent.h"
#include "ConsumableItemData.h"
#include "DropItem.h"
#include "../ProtoCharacter.h"

void UInventoryScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
}

void UInventoryScreenWidget::OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent)
{
	HoveredItemIndex = ItemIndex;

	if (OwningComponent && OwningComponent->Items.IsValidIndex(ItemIndex))
	{
		if (UItemDataBase* ItemData = OwningComponent->Items[ItemIndex].ItemData)
		{
			ShowActionTooltip(ItemData->GetContextActionText());
		}
	}
}

void UInventoryScreenWidget::OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent*)
{
	if (HoveredItemIndex == ItemIndex)
	{
		HoveredItemIndex = INDEX_NONE;
		HideActionTooltip();
	}
}

void UInventoryScreenWidget::ShowActionTooltip(const FText& Text)
{
	if (ActionTooltipWidget)
	{
		ActionTooltipWidget->SetActionText(Text);
	}
}

void UInventoryScreenWidget::HideActionTooltip()
{
	if (ActionTooltipWidget)
	{
		ActionTooltipWidget->SetActionText(FText::GetEmpty());
	}
}

bool UInventoryScreenWidget::OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent*)
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

bool UInventoryScreenWidget::OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent*)
{
	if (!CachedInventoryComponent || !DragOp || !DragOp->SourceInventoryComponent) return false;

	UItemDataBase* ItemData = DragOp->DraggedItemData;
	if (!ItemData) return false;

	FIntPoint ItemSize = bDropRotated
		? FIntPoint(ItemData->GridHeight, ItemData->GridWidth)
		: FIntPoint(ItemData->GridWidth,  ItemData->GridHeight);

	if (!CachedInventoryComponent->CanPlaceAt(TargetPosition, ItemSize)) return false;

	FInventoryItemInstance SourceInstance;
	const int32 StackCount = DragOp->SourceInventoryComponent->FindInstanceById(DragOp->InstanceId, SourceInstance) ? SourceInstance.StackCount : 1;

	DragOp->SourceInventoryComponent->RemoveInstanceById(DragOp->InstanceId);
	CachedInventoryComponent->AddItemAt(ItemData, TargetPosition, bDropRotated, StackCount);

	if (DragOp->SourceScreenWidget)
	{
		DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
	}
	InitializeGrid(CachedInventoryComponent);

	ActiveDragOp = nullptr;
	return true;
}

void UInventoryScreenWidget::InitializeEquipment(UEquipmentComponent* InEquipmentComponent)
{
	CachedEquipmentComponent = InEquipmentComponent;

	if (HelmetSlotWidget) HelmetSlotWidget->InitSlot(this, EEquipmentSlot::Helmet, InEquipmentComponent);
	if (VestSlotWidget) VestSlotWidget->InitSlot(this, EEquipmentSlot::Vest, InEquipmentComponent);
	if (Weapon1SlotWidget) Weapon1SlotWidget->InitSlot(this, EEquipmentSlot::Weapon1, InEquipmentComponent);
	if (Weapon2SlotWidget) Weapon2SlotWidget->InitSlot(this, EEquipmentSlot::Weapon2, InEquipmentComponent);
}

void UInventoryScreenWidget::RefreshEquipmentSlots()
{
	if (HelmetSlotWidget) HelmetSlotWidget->RefreshVisual();
	if (VestSlotWidget) VestSlotWidget->RefreshVisual();
	if (Weapon1SlotWidget) Weapon1SlotWidget->RefreshVisual();
	if (Weapon2SlotWidget) Weapon2SlotWidget->RefreshVisual();
}

void UInventoryScreenWidget::InitializeQuickSlots(UQuickSlotComponent* InQuickSlotComponent)
{
	TArray<UQuickSlotRegisterWidget*> Widgets = {
		QuickSlotWidget0, QuickSlotWidget1, QuickSlotWidget2, QuickSlotWidget3,
		QuickSlotWidget4, QuickSlotWidget5, QuickSlotWidget6, QuickSlotWidget7
	};

	for (int32 i = 0; i < Widgets.Num(); ++i)
	{
		if (Widgets[i])
		{
			Widgets[i]->InitSlot(this, i, InQuickSlotComponent);
		}
	}
}

void UInventoryScreenWidget::RefreshQuickSlots()
{
	if (QuickSlotWidget0) QuickSlotWidget0->RefreshVisual();
	if (QuickSlotWidget1) QuickSlotWidget1->RefreshVisual();
	if (QuickSlotWidget2) QuickSlotWidget2->RefreshVisual();
	if (QuickSlotWidget3) QuickSlotWidget3->RefreshVisual();
	if (QuickSlotWidget4) QuickSlotWidget4->RefreshVisual();
	if (QuickSlotWidget5) QuickSlotWidget5->RefreshVisual();
	if (QuickSlotWidget6) QuickSlotWidget6->RefreshVisual();
	if (QuickSlotWidget7) QuickSlotWidget7->RefreshVisual();
}

void UInventoryScreenWidget::OnItemContextAction(int32 ItemIndex, UInventoryGridComponent* OwningComponent)
{
	if (!OwningComponent || !OwningComponent->Items.IsValidIndex(ItemIndex)) return;

	UItemDataBase* ItemData = OwningComponent->Items[ItemIndex].ItemData;
	if (!ItemData) return;

	AProtoCharacter* OwningCharacter = Cast<AProtoCharacter>(GetOwningPlayerPawn());
	if (!OwningCharacter) return;

	switch (ItemData->GetContextAction())
	{
	case EItemContextAction::Equip:
	{
		UEquipmentComponent* EquipmentComp = OwningCharacter->GetEquipmentComponent();
		EEquipmentSlot TargetSlot;
		if (EquipmentComp && EquipmentComp->ResolveTargetSlot(ItemData, TargetSlot))
		{
			const FGuid InstanceId = OwningComponent->Items[ItemIndex].InstanceId;
			if (EquipmentComp->EquipFromInventory(OwningComponent, InstanceId, TargetSlot))
			{
				RefreshGrid(OwningComponent);
				RefreshEquipmentSlots();
			}
		}
		break;
	}
	case EItemContextAction::Use:
	{
		if (UConsumableItemData* ConsumableData = Cast<UConsumableItemData>(ItemData))
		{
			if (OwningCharacter->UseConsumable(ConsumableData))
			{
				int32 SplitCount = 0;
				OwningComponent->SplitStack(OwningComponent->Items[ItemIndex].InstanceId, 1, SplitCount);

				RefreshGrid(OwningComponent);
			}
		}
		break;
	}
	default:
		break;
	}
}

void UInventoryScreenWidget::SetActiveDragOperation(UItemDragDropOperation* InDragOp)
{
	ActiveDragOp = InDragOp;
}

void UInventoryScreenWidget::RefreshGrid(UInventoryGridComponent* Component)
{
	if (Component) InitializeGrid(Component);
}

FReply UInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		if (ActiveDragOp && ActiveDragOp->DraggedItemData)
		{
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

bool UInventoryScreenWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	ClearDragHighlight();

	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && DragOp->SourceInventoryComponent && DragOp->DraggedItemData)
	{
		DropItemToWorld(DragOp);
		return true;
	}
	return Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
}

void UInventoryScreenWidget::SpawnDropItemActor(UItemDataBase* ItemData, int32 StackCount)
{
	if (!ItemData || !DropItemActorClass) return;

	AProtoCharacter* OwningCharacter = Cast<AProtoCharacter>(GetOwningPlayerPawn());
	if (!OwningCharacter) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector SpawnLocation = OwningCharacter->GetActorLocation() + OwningCharacter->GetActorForwardVector() * 150.f + FVector(0.f, 0.f, 50.f);
	const FTransform SpawnTransform(OwningCharacter->GetActorRotation(), SpawnLocation);

	// ItemData/StackCount가 OnConstruction(메시 세팅) 이전에 반영되어야 하므로 지연 스폰을 사용한다.
	if (ADropItem* Spawned = World->SpawnActorDeferred<ADropItem>(DropItemActorClass, SpawnTransform))
	{
		Spawned->ItemData = ItemData;
		Spawned->StackCount = FMath::Max(1, StackCount);
		Spawned->FinishSpawning(SpawnTransform);
	}
}

void UInventoryScreenWidget::DropItemToWorld(UItemDragDropOperation* DragOp)
{
	if (!DragOp || !DragOp->SourceInventoryComponent || !DragOp->DraggedItemData) return;

	FInventoryItemInstance SourceInstance;
	const int32 FullStackCount = DragOp->SourceInventoryComponent->FindInstanceById(DragOp->InstanceId, SourceInstance) ? SourceInstance.StackCount : 1;

	UItemDataBase* ItemData = DragOp->DraggedItemData;
	DragOp->SourceInventoryComponent->RemoveInstanceById(DragOp->InstanceId);

	SpawnDropItemActor(ItemData, FullStackCount);

	if (DragOp->SourceScreenWidget)
	{
		DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
	}
}

void UInventoryScreenWidget::PerformPartialDrop(UInventoryGridComponent* SourceInventory, const FGuid& InstanceId, int32 Count)
{
	if (!SourceInventory) return;

	int32 SplitCount = 0;
	UItemDataBase* ItemData = SourceInventory->SplitStack(InstanceId, Count, SplitCount);
	if (!ItemData || SplitCount <= 0) return;

	SpawnDropItemActor(ItemData, SplitCount);
	RefreshGrid(SourceInventory);
}

void UInventoryScreenWidget::OnItemRequestPartialDrop(int32 ItemIndex, UInventoryGridComponent* OwningComponent)
{
	if (!OwningComponent || !OwningComponent->Items.IsValidIndex(ItemIndex) || !DropQuantityPopupClass) return;

	const FInventoryItemInstance& Item = OwningComponent->Items[ItemIndex];
	if (!Item.ItemData || !Item.ItemData->bIsStackable || Item.StackCount <= 1) return;

	if (UDropQuantityPopupWidget* Popup = CreateWidget<UDropQuantityPopupWidget>(GetOwningPlayer(), DropQuantityPopupClass))
	{
		Popup->InitPopup(this, OwningComponent, Item.InstanceId, Item.StackCount);
		Popup->AddToViewport(1000);
	}
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
				NewSlotWidget->InitSlot(this, FIntPoint(x, y), InInventoryComponent);
				SlotWidgetMap.Add(FIntPoint(x, y), NewSlotWidget);
				FVector2D SizeFromSlot = NewSlotWidget->GetSlotSizeBoxSize();


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

void UInventoryScreenWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (!IsDesignTime() || !InventoryGridPanel || !SlotWidgetClass) return;

	int32 Columns = 10;
	int32 Rows = 6;

	if (PreviewGridComponentClass)
	{
		const UInventoryGridComponent* CDO = PreviewGridComponentClass->GetDefaultObject<UInventoryGridComponent>();
		Columns = CDO->GridColumns;
		Rows = CDO->GridRows;
	}

	InventoryGridPanel->ClearChildren();

	for (int32 y = 0; y < Rows; ++y)
	{
		for (int32 x = 0; x < Columns; ++x)
		{
			UInventorySlotWidget* NewSlotWidget = CreateWidget<UInventorySlotWidget>(this, SlotWidgetClass);
			if (!NewSlotWidget) continue;

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

void UInventoryScreenWidget::UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent*)
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

void UInventoryScreenWidget::ClearDragHighlight()
{
	for (auto& Pair : SlotWidgetMap)
	{
		Pair.Value->SetHighlight(false, false);
	}
}
