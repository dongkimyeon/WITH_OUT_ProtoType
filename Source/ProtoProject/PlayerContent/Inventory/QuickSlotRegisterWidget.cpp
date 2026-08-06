#include "QuickSlotRegisterWidget.h"
#include "InventoryScreenWidget.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "Components/Border.h"
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
		return;
	}

	UTexture2D* Texture = Entry->ItemData->Icon.LoadSynchronous();
	if (Texture && IconBaseMaterial)
	{
		IconMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, this);
		IconMatInst->SetTextureParameterValue(FName("image"), Texture);
		ItemImage->SetBrushFromMaterial(IconMatInst);
		ItemImage->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

bool UQuickSlotRegisterWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && QuickSlotComponentRef && SlotBorder)
	{
		const bool bValid = QuickSlotComponentRef->CanRegisterToQuickSlot(DragOp->DraggedItemData);
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
	if (DragOp && QuickSlotComponentRef && DragOp->SourceInventoryComponent)
	{
		// 성공/실패와 무관하게 항상 갱신해야, 드래그 시작 시 숨겨졌던 원본 그리드 아이콘이 복원된다.
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
