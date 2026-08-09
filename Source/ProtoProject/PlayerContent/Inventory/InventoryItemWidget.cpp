#include "InventoryItemWidget.h"
#include "InventoryGridComponent.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "InventoryIconUtils.h"
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
		IconMatInst = FInventoryIconUtils::ApplyIcon(ItemImage, IconBaseMaterial, Data->Icon.LoadSynchronous(), this);
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

	FInventoryIconUtils::UpdateStackCountText(StackCountText, Item.ItemData->bIsStackable, Item.StackCount);
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
		if (UMaterialInstanceDynamic* DragMatInst = FInventoryIconUtils::ApplyIcon(DragVisual->ItemImage, IconBaseMaterial, Item.ItemData->Icon.LoadSynchronous(), DragVisual))
		{
			DragMatInst->SetScalarParameterValue(FName("rotation"), Item.bIsRotated ? -0.25f : 0.f);
			DragOp->DragVisualMatInst = DragMatInst;
		}

		// 드래그 프리뷰 수량 텍스트 반영
		FInventoryIconUtils::UpdateStackCountText(DragVisual->StackCountText, Item.ItemData->bIsStackable, Item.StackCount);
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
	// 점유 칸 위 드롭 처리 (그리드 밖 버리기로 오인 방지)
	UItemDragDropOperation* DragOp = Cast<UItemDragDropOperation>(InOperation);
	if (!DragOp || !ParentScreen || !InventoryComponent || !InventoryComponent->Items.IsValidIndex(ItemIndex))
	{
		return false;
	}

	ParentScreen->ClearDragHighlight();

	const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];
	FVector2D CellPixelSize = ParentScreen->GetCellPixelSize();
	FIntPoint ItemGridSize = Item.GetEffectiveSize();

	FVector2D LocalPos = InGeometry.AbsoluteToLocal(InDragDropEvent.GetScreenSpacePosition());
	int32 OffsetX = FMath::Clamp(FMath::FloorToInt(LocalPos.X / CellPixelSize.X), 0, ItemGridSize.X - 1);
	int32 OffsetY = FMath::Clamp(FMath::FloorToInt(LocalPos.Y / CellPixelSize.Y), 0, ItemGridSize.Y - 1);
	FIntPoint HoveredSlot = Item.GridPosition + FIntPoint(OffsetX, OffsetY);
	FIntPoint TargetTopLeft = HoveredSlot - DragOp->DragOffset;

	if (DragOp->SourceEquipmentComponent)
	{
		// 드래그 위치 지정 배치
		if (DragOp->SourceEquipmentComponent->UnequipToInventoryAt(InventoryComponent, DragOp->SourceEquipmentSlot, TargetTopLeft, DragOp->bCurrentRotated))
		{
			ParentScreen->RefreshGrid(InventoryComponent);
		}
		ParentScreen->RefreshEquipmentSlots();
		return true;
	}

	if (DragOp->SourceQuickSlotComponent)
	{
		// 드래그 위치 지정 배치
		if (DragOp->SourceQuickSlotComponent->UnregisterToInventoryAt(DragOp->SourceQuickSlotIndex, InventoryComponent, HoveredSlot, DragOp->bCurrentRotated))
		{
			ParentScreen->RefreshGrid(InventoryComponent);
		}
		ParentScreen->RefreshQuickSlots();
		return true;
	}

	// 같은 아이템이면 배치 대신 스택 병합
	if (DragOp->InstanceId != Item.InstanceId && Item.ItemData && Item.ItemData->bIsStackable && DragOp->DraggedItemData == Item.ItemData)
	{
		if (InventoryComponent->MergeStackFrom(DragOp->SourceInventoryComponent, DragOp->InstanceId, Item.InstanceId))
		{
			// 부분 갱신 (인덱스 밀림 시 전체 재생성)
			ParentScreen->RefreshSingleItem(InventoryComponent, ItemIndex, Item.InstanceId);
			if (DragOp->SourceInventoryComponent && DragOp->SourceInventoryComponent != InventoryComponent && DragOp->SourceScreenWidget)
			{
				DragOp->SourceScreenWidget->RefreshGrid(DragOp->SourceInventoryComponent);
			}
			return true;
		}
		// 병합 실패 시 위치 기반 배치로 폴백
	}

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
