#include "EquipmentSlotWidget.h"
#include "InventoryScreenWidget.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
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

	UTexture2D* Texture = Equipped->ItemData->Icon.LoadSynchronous();
	if (Texture && IconBaseMaterial)
	{
		IconMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, this);
		IconMatInst->SetTextureParameterValue(FName("image"), Texture);
		ItemImage->SetBrushFromMaterial(IconMatInst);
		ItemImage->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		// 아이콘 텍스처가 없으면 이전에 표시되던 다른 아이템의 이미지가 남아있지 않도록 숨긴다.
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
	}
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
		// 다른 장착 슬롯(예: 무기1 <-> 무기2)에서 드래그된 경우 - 스왑/이동.
		// 성공/실패와 무관하게 항상 갱신해야, 드래그 시작 시 숨겨졌던 원본 슬롯의 아이콘이 복원된다.
		EquipmentComponentRef->SwapOrMoveSlots(DragOp->SourceEquipmentSlot, Slot);
		if (ParentScreen) ParentScreen->RefreshEquipmentSlots();
		else RefreshVisual();
		return true;
	}

	if (DragOp->SourceInventoryComponent)
	{
		// 성공/실패와 무관하게 항상 갱신해야, 드래그 시작 시 숨겨졌던 인벤토리 그리드의 원본 아이콘이 복원된다.
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
	if (UTexture2D* Texture = Equipped.ItemData->Icon.LoadSynchronous())
	{
		if (IconBaseMaterial)
		{
			UMaterialInstanceDynamic* DragMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, DragVisual);
			DragMatInst->SetTextureParameterValue(FName("image"), Texture);
			DragVisual->SetBrushFromMaterial(DragMatInst);
		}
	}

	USizeBox* DragWrapper = NewObject<USizeBox>(this);
	DragWrapper->SetWidthOverride(64.f);
	DragWrapper->SetHeightOverride(64.f);
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
