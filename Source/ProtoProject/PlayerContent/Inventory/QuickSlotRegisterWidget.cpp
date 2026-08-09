#include "QuickSlotRegisterWidget.h"
#include "InventoryScreenWidget.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "InventoryIconUtils.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UQuickSlotRegisterWidget::InitSlot(UInventoryScreenWidget* InParentScreen, int32 InSlotIndex, UQuickSlotComponent* InQuickSlotComponent)
{
	ParentScreen = InParentScreen;
	SlotIndex = InSlotIndex;
	QuickSlotComponentRef = InQuickSlotComponent;

	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
	RefreshVisual();
}

void UQuickSlotRegisterWidget::RefreshVisual()
{
	if (!ItemImage) return;

	const FQuickSlotEntry* Entry = QuickSlotComponentRef ? &QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex) : nullptr;
	if (!Entry || !Entry->ItemData)
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
		if (StackCountText) StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	IconMatInst = FInventoryIconUtils::ApplyIcon(ItemImage, IconBaseMaterial, Entry->ItemData->Icon.LoadSynchronous(), this);
	FInventoryIconUtils::UpdateStackCountText(StackCountText, Entry->ItemData->bIsStackable, Entry->StackCount);
}

bool UQuickSlotRegisterWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && QuickSlotComponentRef && SlotBorder)
	{
		bool bValid;
		if (DragOp->SourceQuickSlotComponent == QuickSlotComponentRef)
		{
			// 자기 자신 위는 무효, 그 외엔 스왑 가능
			bValid = DragOp->SourceQuickSlotIndex != SlotIndex;
		}
		else
		{
			bValid = QuickSlotComponentRef->CanRegisterToQuickSlot(DragOp->DraggedItemData);
		}
		SlotBorder->SetBrushColor(bValid ? ValidColor : InvalidColor);
		return true;
	}
	return false;
}

void UQuickSlotRegisterWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}
}

bool UQuickSlotRegisterWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	if (SlotBorder)
	{
		SlotBorder->SetBrushColor(DefaultColor);
	}

	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (!DragOp || !QuickSlotComponentRef) return false;

	if (DragOp->SourceQuickSlotComponent == QuickSlotComponentRef && DragOp->SourceQuickSlotIndex != SlotIndex)
	{
		// 퀵슬롯 칸 스왑
		QuickSlotComponentRef->SwapOrMoveSlots(DragOp->SourceQuickSlotIndex, SlotIndex);
		if (ParentScreen) ParentScreen->RefreshQuickSlots();
		else RefreshVisual();
		return true;
	}

	if (DragOp->SourceInventoryComponent)
	{
		// 인벤토리 → 퀵슬롯 등록
		QuickSlotComponentRef->RegisterFromInventory(SlotIndex, DragOp->SourceInventoryComponent, DragOp->InstanceId);
		RefreshVisual();
		if (DragOp->SourceScreenWidget)
		{
			DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
		}
		return true;
	}

	return false;
}

FReply UQuickSlotRegisterWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		if (QuickSlotComponentRef && QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex).ItemData)
		{
			return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
		}
		return FReply::Unhandled();
	}

	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton) && QuickSlotComponentRef && ParentScreen)
	{
		if (UInventoryGridComponent* TargetInventory = ParentScreen->GetCachedInventoryComponent())
		{
			if (QuickSlotComponentRef->UnregisterToInventory(SlotIndex, TargetInventory))
			{
				RefreshVisual();
				ParentScreen->RefreshGrid(TargetInventory);
			}
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void UQuickSlotRegisterWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!QuickSlotComponentRef) return;

	const FQuickSlotEntry& Entry = QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex);
	if (!Entry.ItemData) return;

	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
	DragOp->DraggedItemData = Entry.ItemData;
	DragOp->SourceQuickSlotComponent = QuickSlotComponentRef;
	DragOp->SourceQuickSlotIndex = SlotIndex;
	DragOp->SourceScreenWidget = ParentScreen;

	UImage* DragVisual = NewObject<UImage>(this);
	FInventoryIconUtils::ApplyIcon(DragVisual, IconBaseMaterial, Entry.ItemData->Icon.LoadSynchronous(), DragVisual);

	// 드래그 아이콘 크기를 슬롯 크기에 맞춤
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
	if (StackCountText) StackCountText->SetVisibility(ESlateVisibility::Collapsed);
	OutOperation = DragOp;
}

void UQuickSlotRegisterWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	RefreshVisual();
}

void UQuickSlotRegisterWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);

	if (ParentScreen && QuickSlotComponentRef && QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex).ItemData)
	{
		ParentScreen->ShowActionTooltip(NSLOCTEXT("Item", "ContextAction_UnregisterQuickSlot", "등록 해제"));
	}
}

void UQuickSlotRegisterWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);

	if (ParentScreen)
	{
		ParentScreen->HideActionTooltip();
	}
}
