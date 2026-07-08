#include "InventoryItemWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"
#include "ItemDragDropOperation.h"
#include "ItemDataBase.h"
#include "Engine/Texture2D.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"

void UInventoryItemWidget::InitItem(UInventoryScreenWidget* InParentScreen, UInventoryGridComponent* InComponent, int32 InItemIndex)
{
	ParentScreen = InParentScreen;
	InventoryComponent = InComponent;
	ItemIndex = InItemIndex;

	if (ItemImage && InComponent && InComponent->Items.IsValidIndex(InItemIndex))
	{
		if (UItemDataBase* Data = InComponent->Items[InItemIndex].ItemData)
		{
			if (UTexture2D* Texture = Data->Icon.LoadSynchronous())
			{
				ItemImage->SetBrushFromTexture(Texture, false);
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

	FVector2D CellPixelSize = ParentScreen ? ParentScreen->GetCellPixelSize() : FVector2D(75.f, 75.f);
	
	FVector2D OriginalPixelSize(Item.ItemData->GridWidth * CellPixelSize.X, Item.ItemData->GridHeight * CellPixelSize.Y);
	ItemImage->SetBrushSize(OriginalPixelSize);
	ItemImage->SetRenderTransformAngle(Item.bIsRotated ? 90.f : 0.f);
}

FReply UInventoryItemWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UInventoryItemWidget::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation)
{
	if (!InventoryComponent || !InventoryComponent->Items.IsValidIndex(ItemIndex)) return;

	const FInventoryItemInstance& Item = InventoryComponent->Items[ItemIndex];

	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
	DragOp->ItemIndex = ItemIndex;
	DragOp->DraggedItemData = Item.ItemData;
	DragOp->OriginalPosition = Item.GridPosition;
	DragOp->bOriginalRotated = Item.bIsRotated;

	FVector2D CellPixelSize = ParentScreen ? ParentScreen->GetCellPixelSize() : FVector2D(75.f, 75.f);
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
			DragVisual->ItemImage->SetBrushFromTexture(Texture, false);
			FVector2D OriginalPixelSize(Item.ItemData->GridWidth * CellPixelSize.X, Item.ItemData->GridHeight * CellPixelSize.Y);
			DragVisual->ItemImage->SetBrushSize(OriginalPixelSize);
			DragVisual->ItemImage->SetRenderTransformAngle(Item.bIsRotated ? 90.f : 0.f);
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

	DragOp->DefaultDragVisual = DragWrapper;
	DragOp->Pivot = EDragPivot::MouseDown;
	SetVisibility(ESlateVisibility::Hidden);
	OutOperation = DragOp;
}

void UInventoryItemWidget::NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	Super::NativeOnDragCancelled(InDragDropEvent, InOperation);
	SetVisibility(ESlateVisibility::Visible);
}

void UInventoryItemWidget::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	if (ParentScreen) ParentScreen->OnItemHoverBegin(ItemIndex);
}

void UInventoryItemWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	if (ParentScreen) ParentScreen->OnItemHoverEnd(ItemIndex);
}