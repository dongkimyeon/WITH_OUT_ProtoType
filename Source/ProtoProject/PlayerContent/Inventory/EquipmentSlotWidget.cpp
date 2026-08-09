#include "EquipmentSlotWidget.h"
#include "InventoryScreenWidget.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "InventoryIconUtils.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UEquipmentSlotWidget::InitSlot(UInventoryScreenWidget* InParentScreen, EEquipmentSlot InSlot, UEquipmentComponent* InEquipmentComponent)
{
	ParentScreen = InParentScreen;
	Slot = InSlot;
	EquipmentComponentRef = InEquipmentComponent;

	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
	RefreshVisual();
}

void UEquipmentSlotWidget::RefreshVisual()
{
	if (!ItemImage) return;

	const FEquippedItem* Equipped = EquipmentComponentRef ? &EquipmentComponentRef->GetEquippedItem(Slot) : nullptr;
	if (!Equipped || !Equipped->ItemData)
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
		return;
	}

	IconMatInst = FInventoryIconUtils::ApplyIcon(ItemImage, IconBaseMaterial, Equipped->ItemData->Icon.LoadSynchronous(), this);
}

bool UEquipmentSlotWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && EquipmentComponentRef && SlotBorder)
	{
		bool bValid;
		if (DragOp->SourceEquipmentComponent == EquipmentComponentRef)
		{
			// 다른 장착 슬롯에서 시작된 드래그 - 자기 자신 위는 무효, 그 외엔 타입 매칭만 검사 (스왑)
			bValid = (DragOp->SourceEquipmentSlot != Slot) && EquipmentComponentRef->CanEquipToSlot(DragOp->DraggedItemData, Slot);
		}
		else
		{
			bValid = EquipmentComponentRef->CanEquipToSlot(DragOp->DraggedItemData, Slot);
		}
		SlotBorder->SetBrushColor(bValid ? ValidColor : InvalidColor);
		return true;
	}
	return false;
}

void UEquipmentSlotWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
}

bool UEquipmentSlotWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}

	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (!DragOp || !EquipmentComponentRef) return false;

	if (DragOp->SourceEquipmentComponent == EquipmentComponentRef && DragOp->SourceEquipmentSlot != Slot)
	{
		EquipmentComponentRef->SwapOrMoveSlots(DragOp->SourceEquipmentSlot, Slot);
		if (ParentScreen) ParentScreen->RefreshEquipmentSlots();
		else RefreshVisual();
		return true;
	}

	if (DragOp->SourceInventoryComponent)
	{
		EquipmentComponentRef->EquipFromInventory(DragOp->SourceInventoryComponent, DragOp->InstanceId, Slot);
		RefreshVisual();
		if (DragOp->SourceScreenWidget)
		{
			DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
		}
		return true;
	}

	return false;
}

FReply UEquipmentSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		if (EquipmentComponentRef && EquipmentComponentRef->GetEquippedItem(Slot).ItemData)
		{
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
		return FReply::Unhandled();
	}

	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && EquipmentComponentRef && ParentScreen)
	{
		if (UInventoryGridComponent* TargetInventory = ParentScreen->GetCachedInventoryComponent())
		{
			if (EquipmentComponentRef->UnequipToInventory(TargetInventory, Slot))
			{
				RefreshVisual();
				ParentScreen->RefreshGrid(TargetInventory);
			}
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UEquipmentSlotWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!EquipmentComponentRef) return;

	const FEquippedItem& Equipped = EquipmentComponentRef->GetEquippedItem(Slot);
	if (!Equipped.ItemData) return;

	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
	DragOp->DraggedItemData = Equipped.ItemData;
	DragOp->SourceEquipmentComponent = EquipmentComponentRef;
	DragOp->SourceEquipmentSlot = Slot;
	DragOp->SourceScreenWidget = ParentScreen;

	UImage* DragVisual = NewObject<UImage>(this);
	FInventoryIconUtils::ApplyIcon(DragVisual, IconBaseMaterial, Equipped.ItemData->Icon.LoadSynchronous(), DragVisual);

	const FVector2D SlotSize = InGeometry.GetLocalSize();

	USizeBox* DragWrapper = NewObject<USizeBox>(this);
	DragWrapper->SetWidthOverride(SlotSize.X);
	DragWrapper->SetHeightOverride(SlotSize.Y);
	if (USizeBoxSlot* WrapperSlot = Cast<USizeBoxSlot>(DragWrapper->AddChild(DragVisual)))
	{
		WrapperSlot->SetHorizontalAlignment(HAlign_Fill);
		WrapperSlot->SetVerticalAlignment(VAlign_Fill);
	}

	DragOp->DefaultDragVisual = DragWrapper;
	DragOp->Pivot = EDragPivot::MouseDown;

	if (ItemImage) ItemImage->SetVisibility(ESlateVisibility::Hidden);
	OutOperation = DragOp;
}

void UEquipmentSlotWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	RefreshVisual();
}

void UEquipmentSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (ParentScreen && EquipmentComponentRef && EquipmentComponentRef->GetEquippedItem(Slot).ItemData)
	{
		ParentScreen->ShowActionTooltip(NSLOCTEXT("Item", "ContextAction_Unequip", "해제"));
	}
}

void UEquipmentSlotWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (ParentScreen)
	{
		ParentScreen->HideActionTooltip();
	}
}
