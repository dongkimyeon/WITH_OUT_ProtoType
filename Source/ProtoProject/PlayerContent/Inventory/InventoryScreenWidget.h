#pragma once

#include "CoreMinimal.h"
#include "InventoryScreenBase.h"
#include "InventoryItemWidget.h"
#include "InventorySlotWidget.h"
#include "ItemDragDropOperation.h"
#include "InventoryGridComponent.h"
#include "InventoryScreenWidget.generated.h"

class UGridPanel;
class UItemDataBase;
class UItemDragDropOperation;

UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UInventoryScreenWidget : public UInventoryScreenBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeGrid(UInventoryGridComponent* InInventoryComponent);

	virtual void NativePreConstruct() override;

	virtual FVector2D GetCellPixelSize() const override { return SlotPixelSize; }
	virtual void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent) override;
	virtual void ClearDragHighlight() override;
	virtual bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual bool OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual void SetActiveDragOperation(UItemDragDropOperation* InDragOp) override;
	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void RefreshGrid(UInventoryGridComponent* Component) override;

	UInventoryGridComponent* GetCachedInventoryComponent() const { return CachedInventoryComponent; }

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UGridPanel* InventoryGridPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventoryItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FVector2D SlotPixelSize = FVector2D(75.f, 75.f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventoryGridComponent> PreviewGridComponentClass;

private:
	void RefreshItemWidget(int32 ItemIndex);

	UPROPERTY()
	UInventoryGridComponent* CachedInventoryComponent = nullptr;

	UPROPERTY()
	TArray<UInventoryItemWidget*> ItemWidgets;

	UPROPERTY()
	TMap<FIntPoint, UInventorySlotWidget*> SlotWidgetMap;

	int32 HoveredItemIndex = INDEX_NONE;

	UPROPERTY()
	UItemDragDropOperation* ActiveDragOp = nullptr;
};
