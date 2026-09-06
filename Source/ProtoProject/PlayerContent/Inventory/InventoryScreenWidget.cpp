#include "InventoryScreenWidget.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "ItemDragDropOperation.h"
#include "InputCoreTypes.h"
#include "EquipmentComponent.h"
#include "ConsumableItemData.h"
#include "DropItem.h"
#include "../ProtoCharacter.h"
#include "../../Network/ProtoNetClientSubsystem.h"
#include "Engine/GameInstance.h"

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
		RefreshItemWidgetInMap(ItemIndex, CachedInventoryComponent, ItemWidgets);
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
	const bool bFoundSource = DragOp->SourceInventoryComponent->FindInstanceById(DragOp->InstanceId, SourceInstance);
	const int32 StackCount = bFoundSource ? SourceInstance.StackCount : 1;

	// NetSlotId != 0 means the source item lives in a world-shared
	// container (see FInventoryItemInstance::NetSlotId's comment) --
	// another player could be dragging the exact same item right now, so
	// ask the server first instead of moving it immediately. Falls through
	// to the old immediate path if not connected: no one else to race
	// against, and a round trip that will never answer would otherwise mean
	// the item can never be taken at all in that mode.
	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	if (bFoundSource && SourceInstance.NetSlotId != 0 && NetClient && NetClient->IsConnected())
	{
		FPendingExternalTransfer Pending;
		Pending.SourceInventory = DragOp->SourceInventoryComponent;
		Pending.TargetInventory = CachedInventoryComponent;
		Pending.SourceScreenWidget = DragOp->SourceScreenWidget;
		Pending.SourceInstanceId = DragOp->InstanceId;
		Pending.TargetPosition = TargetPosition;
		Pending.bTargetRotated = bDropRotated;
		Pending.ItemData = ItemData;
		Pending.StackCount = StackCount;
		PendingExternalTransfers.Add(SourceInstance.NetSlotId, Pending);

		NetClient->OnItemPickupResult.AddUniqueDynamic(this, &UInventoryScreenWidget::HandleExternalTransferPickupResult);
		NetClient->SendInteractLoot(SourceInstance.NetSlotId);

		// Leaves both grids showing the item until the server answers (see
		// HandleExternalTransferPickupResult) -- same trade-off
		// ADropItem::RequestPickup accepts for ground items: a brief,
		// harmless visual inconsistency during the round trip beats
		// optimistically moving something that might not actually be ours.
		ActiveDragOp = nullptr;
		return true;
	}

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

void UInventoryScreenWidget::HandleExternalTransferPickupResult(int32 NetSlotId, int32 PickerPlayerId, bool bGranted)
{
	FPendingExternalTransfer Pending;
	if (!PendingExternalTransfers.RemoveAndCopyValue(NetSlotId, Pending))
	{
		// Every screen showing a world-shared grid binds this same
		// broadcast delegate -- not our transfer.
		return;
	}

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UProtoNetClientSubsystem* NetClient = GameInstance ? GameInstance->GetSubsystem<UProtoNetClientSubsystem>() : nullptr;
	// bGranted false (Denied) means this was never a race to begin with --
	// bGrantedToMe short-circuits to false without even checking PickerPlayerId.
	const bool bGrantedToMe = bGranted && NetClient && PickerPlayerId == NetClient->GetLocalPlayerId();

	UInventoryGridComponent* SourceInventory = Pending.SourceInventory.Get();
	if (SourceInventory)
	{
		// Gone from the shared container either way -- someone got it, even
		// if it wasn't us.
		SourceInventory->RemoveInstanceById(Pending.SourceInstanceId);
	}

	if (bGrantedToMe)
	{
		if (UInventoryGridComponent* TargetInventory = Pending.TargetInventory.Get())
		{
			TargetInventory->AddItemAt(Pending.ItemData, Pending.TargetPosition, Pending.bTargetRotated, Pending.StackCount);
		}
	}

	if (UInventoryScreenBase* SourceScreen = Pending.SourceScreenWidget.Get())
	{
		SourceScreen->RefreshGrid(SourceInventory);
	}
	RefreshGrid(Pending.TargetInventory.Get());

	// Stop listening only once nothing else is outstanding -- avoids
	// dropping another concurrent transfer's eventual answer.
	if (PendingExternalTransfers.Num() == 0 && NetClient)
	{
		NetClient->OnItemPickupResult.RemoveDynamic(this, &UInventoryScreenWidget::HandleExternalTransferPickupResult);
	}
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
				const FGuid ConsumedInstanceId = OwningComponent->Items[ItemIndex].InstanceId;
				int32 SplitCount = 0;
				OwningComponent->SplitStack(ConsumedInstanceId, 1, SplitCount);

				if (!TryLightRefresh(OwningComponent, ItemIndex, ConsumedInstanceId, ItemWidgets))
				{
					RefreshGrid(OwningComponent);
				}
			}
		}
		break;
	}
	default:
		break;
	}
}

void UInventoryScreenWidget::RefreshGrid(UInventoryGridComponent* Component)
{
	if (Component) InitializeGrid(Component);
}

void UInventoryScreenWidget::RefreshSingleItem(UInventoryGridComponent* Component, int32 ItemIndex, const FGuid& ExpectedInstanceId)
{
	if (Component != CachedInventoryComponent) return;

	if (!TryLightRefresh(Component, ItemIndex, ExpectedInstanceId, ItemWidgets))
	{
		// 인덱스 밀림 시 전체 재생성
		RefreshGrid(Component);
	}
}

FReply UInventoryScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::R)
	{
		if (HandleRotateDragKey())
		{
			return FReply::Handled();
		}

		if (HoveredItemIndex != INDEX_NONE)
		{
			if (CachedInventoryComponent && CachedInventoryComponent->RotateItem(HoveredItemIndex))
			{
				RefreshItemWidgetInMap(HoveredItemIndex, CachedInventoryComponent, ItemWidgets);
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

	// FTransform(Rotation, Location) 생성자는 스케일을 (1,1,1)로 고정한다. SpawnActorDeferred +
	// FinishSpawning은 이 트랜스폼을 루트 컴포넌트에 그대로 적용해버려서, 블루프린트에서
	// StaticMeshComp에 설정해둔 스케일 값이 스폰되는 순간 항상 1로 덮어써진다 - 클래스 기본값(CDO)의
	// 스케일을 읽어와 그대로 유지시킨다.
	FVector DefaultScale = FVector::OneVector;
	if (const ADropItem* DefaultDropItem = GetDefault<ADropItem>(DropItemActorClass))
	{
		if (DefaultDropItem->StaticMeshComp)
		{
			DefaultScale = DefaultDropItem->StaticMeshComp->GetRelativeScale3D();
		}
	}
	const FTransform SpawnTransform(OwningCharacter->GetActorRotation(), SpawnLocation, DefaultScale);

	// 지연 스폰 (메시 세팅 전 데이터 반영)
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

void UInventoryScreenWidget::InitializeGrid(UInventoryGridComponent* InInventoryComponent)
{
	if (!InInventoryComponent || !InventoryGridPanel || !SlotWidgetClass)
	{
		return;
	}

	CachedInventoryComponent = InInventoryComponent;
	HoveredItemIndex = INDEX_NONE;

	PopulateGridPanel(InventoryGridPanel, InInventoryComponent, SlotWidgetClass, ItemWidgetClass, SlotWidgetMap, ItemWidgets);
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

	ApplyDragHighlightToMap(SlotWidgetMap, TargetTopLeft, Size, bIsValid);
}

void UInventoryScreenWidget::ClearDragHighlight()
{
	ClearHighlightMap(SlotWidgetMap);
}
