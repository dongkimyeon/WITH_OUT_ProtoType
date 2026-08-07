#include "InventoryItemWidget.h"
#include "InventoryGridComponent.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "Engine/Texture2D.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"

void UInventoryItemWidget::InitItem(UInventoryScreenBase* InParentScreen, UInventoryGridComponent* InComponent, int32 InItemIndex)
{
	ParentScreen = InParentScreen;
	InventoryComponent = InComponent;
	ItemIndex = InItemIndex;

	if (!InComponent || !InComponent->Items.IsValidIndex(InItemIndex)) return;

	if (UItemDataBase* Data = InComponent->Items[InItemIndex].ItemData)
	{
		if (UTexture2D* Texture = Data->Icon.LoadSynchronous())
		{
			if (IconBaseMaterial)
			{
				IconMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, this);
				IconMatInst->SetTextureParameterValue(FName("image"), Texture);
				ItemImage->SetBrushFromMaterial(IconMatInst);
			}
		}
	}
	RefreshVisual();
}

void UInventoryItemWidget::RefreshVisual()
{
	if (!InventoryComponent || !InventoryComponent->Items.IsValidIndex(ItemIndex)) return;

	const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];
	if (!Item.ItemData || !ItemImage) return;

	if (IconMatInst)
	{
		IconMatInst->SetScalarParameterValue(FName("rotation"), Item.bIsRotated ? -0.25f : 0.f);
	}

	if (StackCountText)
	{
		if (Item.ItemData->bIsStackable && Item.StackCount > 1)
		{
			StackCountText->SetText(FText::AsNumber(Item.StackCount));
			StackCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

FReply UInventoryItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	if (InMouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		if (ParentScreen)
		{
			if (InMouseEvent.IsControlDown())
			{
				ParentScreen->OnItemRequestPartialDrop(ItemIndex, InventoryComponent);
			}
			else
			{
				ParentScreen->OnItemContextAction(ItemIndex, InventoryComponent);
			}
		}
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!InventoryComponent || !InventoryComponent->Items.IsValidIndex(ItemIndex)) return;

	const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];

	FVector2D CellPixelSize = ParentScreen ? ParentScreen->GetCellPixelSize() : FVector2D(100.f, 100.f);

	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
	DragOp->ItemIndex = ItemIndex;
	DragOp->InstanceId = Item.InstanceId;
	DragOp->DraggedItemData = Item.ItemData;
	DragOp->OriginalPosition = Item.GridPosition;
	DragOp->bOriginalRotated = Item.bIsRotated;
	DragOp->bCurrentRotated = Item.bIsRotated;
	DragOp->CellPixelSize = CellPixelSize;
	DragOp->SourceInventoryComponent = InventoryComponent;
	DragOp->SourceScreenWidget = ParentScreen;

	FIntPoint ItemGridSize = Item.GetEffectiveSize();

	FVector2D LocalClickPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
	int32 ClickedX = FMath::Clamp(FMath::FloorToInt(LocalClickPos.X / CellPixelSize.X), 0, ItemGridSize.X - 1);
	int32 ClickedY = FMath::Clamp(FMath::FloorToInt(LocalClickPos.Y / CellPixelSize.Y), 0, ItemGridSize.Y - 1);

	DragOp->DragOffset = FIntPoint(ClickedX, ClickedY);

	FVector2D VisualSize(CellPixelSize.X * ItemGridSize.X, CellPixelSize.Y * ItemGridSize.Y);

	UInventoryItemWidget* DragVisual = CreateWidget<UInventoryItemWidget>(GetOwningPlayer(), GetClass());
	if (DragVisual && DragVisual->ItemImage && Item.ItemData)
	{
		if (UTexture2D* Texture = Item.ItemData->Icon.LoadSynchronous())
		{
			if (IconBaseMaterial)
			{
				UMaterialInstanceDynamic* DragMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, DragVisual);
				DragMatInst->SetTextureParameterValue(FName("image"), Texture);
				DragMatInst->SetScalarParameterValue(FName("rotation"), Item.bIsRotated ? -0.25f : 0.f);
				DragVisual->ItemImage->SetBrushFromMaterial(DragMatInst);
				DragOp->DragVisualMatInst = DragMatInst;
			}
		}

		// 드래그 프리뷰는 InitItem()을 거치지 않으므로 수량 텍스트를 직접 반영해야 디자인 타임 기본값이 그대로 노출되지 않는다.
		if (DragVisual->StackCountText)
		{
			if (Item.ItemData->bIsStackable && Item.StackCount > 1)
			{
				DragVisual->StackCountText->SetText(FText::AsNumber(Item.StackCount));
				DragVisual->StackCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				DragVisual->StackCountText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	USizeBox* DragWrapper = NewObject<USizeBox>(this);
	DragWrapper->SetWidthOverride(VisualSize.X);
	DragWrapper->SetHeightOverride(VisualSize.Y);

	if (USizeBoxSlot* WrapperSlot = Cast<USizeBoxSlot>(DragWrapper->AddChild(DragVisual)))
	{
		WrapperSlot->SetHorizontalAlignment(HAlign_Fill);
		WrapperSlot->SetVerticalAlignment(VAlign_Fill);
	}

	DragOp->DragVisualImage = DragVisual ? DragVisual->ItemImage : nullptr;
	DragOp->DragVisualWrapper = DragWrapper;
	DragOp->DefaultDragVisual = DragWrapper;
	DragOp->Pivot = EDragPivot::MouseDown;
	SetVisibility(ESlateVisibility::Hidden);
	if (ParentScreen) ParentScreen->SetActiveDragOperation(DragOp);
	OutOperation = DragOp;
}

void UInventoryItemWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	SetVisibility(ESlateVisibility::Visible);
	if (ParentScreen) ParentScreen->SetActiveDragOperation(nullptr);
}

bool UInventoryItemWidget::NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (DragOp && ParentScreen && InventoryComponent && InventoryComponent->Items.IsValidIndex(ItemIndex))
	{
		const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];
		FVector2D CellPixelSize = ParentScreen->GetCellPixelSize();
		FIntPoint ItemGridSize = Item.GetEffectiveSize();

		FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
		int32 OffsetX = FMath::Clamp(FMath::FloorToInt(LocalPos.X / CellPixelSize.X), 0, ItemGridSize.X - 1);
		int32 OffsetY = FMath::Clamp(FMath::FloorToInt(LocalPos.Y / CellPixelSize.Y), 0, ItemGridSize.Y - 1);

		FIntPoint HoveredSlot = Item.GridPosition + FIntPoint(OffsetX, OffsetY);
		FIntPoint TargetTopLeft = HoveredSlot - DragOp->DragOffset;

		bool bCrossGrid = DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != InventoryComponent;
		int32 IgnoreIdx = bCrossGrid ? INDEX_NONE : DragOp->ItemIndex;

		ParentScreen->UpdateDragHighlight(TargetTopLeft, DragOp->DraggedItemData, DragOp->bCurrentRotated, IgnoreIdx, InventoryComponent);
		return true;
	}
	return false;
}

void UInventoryItemWidget::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragLeave(InDragDropEvent, InOperation);
	if (ParentScreen) ParentScreen->ClearDragHighlight();
}

bool UInventoryItemWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	// 이미 아이템이 있는 칸(=배치 무효/빨간 하이라이트) 위에 드롭하면 이 위젯이 커서를 가로챈다.
	// 여기서 처리하지 않으면 이벤트가 상위로 전파되어 "그리드 밖 버리기"로 오인되므로 슬롯과 동일하게 처리한다.
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (!DragOp || !ParentScreen || !InventoryComponent || !InventoryComponent->Items.IsValidIndex(ItemIndex))
	{
		return false;
	}

	ParentScreen->ClearDragHighlight();

	if (DragOp->SourceEquipmentComponent)
	{
		if (DragOp->SourceEquipmentComponent->UnequipToInventory(InventoryComponent, DragOp->SourceEquipmentSlot))
		{
			ParentScreen->RefreshGrid(InventoryComponent);
		}
		ParentScreen->RefreshEquipmentSlots();
		return true;
	}

	const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];
	FVector2D CellPixelSize = ParentScreen->GetCellPixelSize();
	FIntPoint ItemGridSize = Item.GetEffectiveSize();

	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	int32 OffsetX = FMath::Clamp(FMath::FloorToInt(LocalPos.X / CellPixelSize.X), 0, ItemGridSize.X - 1);
	int32 OffsetY = FMath::Clamp(FMath::FloorToInt(LocalPos.Y / CellPixelSize.Y), 0, ItemGridSize.Y - 1);
	FIntPoint HoveredSlot = Item.GridPosition + FIntPoint(OffsetX, OffsetY);

	if (DragOp->SourceQuickSlotComponent)
	{
		// 우클릭 해제(자동 배치)와 달리 드롭한 정확한 위치에 배치한다.
		if (DragOp->SourceQuickSlotComponent->UnregisterToInventoryAt(DragOp->SourceQuickSlotIndex, InventoryComponent, HoveredSlot, DragOp->bCurrentRotated))
		{
			ParentScreen->RefreshGrid(InventoryComponent);
		}
		ParentScreen->RefreshQuickSlots();
		return true;
	}

	// 겹칠 수 있는 같은 아이템 위에 놓으면 위치 배치 대신 수량을 합친다.
	if (DragOp->InstanceId != Item.InstanceId && Item.ItemData && Item.ItemData->bIsStackable && DragOp->DraggedItemData == Item.ItemData)
	{
		if (InventoryComponent->MergeStackFrom(DragOp->SourceInventoryComponent, DragOp->InstanceId, Item.InstanceId))
		{
			ParentScreen->RefreshGrid(InventoryComponent);
			if (DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != InventoryComponent && DragOp->SourceScreenWidget)
			{
				DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
			}
			return true;
		}
		// 병합이 불가능하면(꽉 참 등) 기존처럼 위치 기반 배치를 시도한다.
	}

	FIntPoint TargetTopLeft = HoveredSlot - DragOp->DragOffset;

	bool bCrossGrid = DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != InventoryComponent;

	if (bCrossGrid)
	{
		if (!ParentScreen->OnItemDroppedFromExternal(DragOp, TargetTopLeft, DragOp->bCurrentRotated, InventoryComponent))
		{
			if (DragOp->SourceScreenWidget)
			{
				DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
			}
		}
		return true;
	}

	if (!ParentScreen->OnItemDropped(DragOp->ItemIndex, TargetTopLeft, DragOp->bCurrentRotated, InventoryComponent))
	{
		ParentScreen->RefreshGrid(InventoryComponent);
	}
	return true;
}

void UInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (ParentScreen) ParentScreen->OnItemHoverBegin(ItemIndex, InventoryComponent);
}

void UInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (ParentScreen) ParentScreen->OnItemHoverEnd(ItemIndex, InventoryComponent);
}
