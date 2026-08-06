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

	FVector2D CellPixelSize = ParentScreen ? ParentScreen->GetCellPixelSize() : FVector2D(100.f, 100.f);

	UItemDragDropOperation* DragOp = NewObject<UItemDragDropOperation>(this);
	DragOp->ItemIndex = ItemIndex;
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
