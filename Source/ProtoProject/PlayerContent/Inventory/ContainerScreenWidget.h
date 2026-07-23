#pragma once

#include "CoreMinimal.h"
#include "InventoryScreenBase.h"
#include "InventorySlotWidget.h"
#include "InventoryItemWidget.h"
#include "InventoryGridComponent.h"
#include "ContainerScreenWidget.generated.h"

class UGridPanel;
class UItemDragDropOperation;

UCLASS(meta = (PrioritizeCategories = "Container UI"))
class PROTOPROJECT_API UContainerScreenWidget : public UInventoryScreenBase
{
	GENERATED_BODY()

public:
	void InitializeScreen(UInventoryGridComponent* PlayerComp, UInventoryGridComponent* ContainerComp);

	virtual FVector2D GetCellPixelSize() const override { return SlotPixelSize; }
	virtual void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent) override;
	virtual void ClearDragHighlight() override;
	virtual bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual bool OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual void SetActiveDragOperation(UItemDragDropOperation* InDragOp) override;
	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void RefreshGrid(UInventoryGridComponent* Component) override;

protected:
	virtual void NativeConstruct() override;
	virtual void NativePreConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UGridPanel* PlayerGridPanel;

	UPROPERTY(meta = (BindWidget))
	UGridPanel* ContainerGridPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Container UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Container UI")
	TSubclassOf<UInventoryItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Container UI")
	FVector2D SlotPixelSize = FVector2D(75.f, 75.f);

	UPROPERTY(EditDefaultsOnly, Category = "Container UI")
	TSubclassOf<UInventoryGridComponent> PreviewPlayerGridClass;

	UPROPERTY(EditDefaultsOnly, Category = "Container UI")
	TSubclassOf<UInventoryGridComponent> PreviewContainerGridClass;

private:
	void PopulateGrid(UGridPanel* GridPanel, UInventoryGridComponent* Comp,
		TMap<FIntPoint, UInventorySlotWidget*>& OutSlotMap, TArray<UInventoryItemWidget*>& OutItemWidgets);
	void RefreshItemWidgetInGrid(int32 ItemIndex, UInventoryGridComponent* Comp,
		TArray<UInventoryItemWidget*>& ItemWidgets);

	UPROPERTY() UInventoryGridComponent* PlayerInventoryComponent = nullptr;
	UPROPERTY() UInventoryGridComponent* ContainerInventoryComponent = nullptr;

	UPROPERTY() TMap<FIntPoint, UInventorySlotWidget*> PlayerSlotMap;
	UPROPERTY() TMap<FIntPoint, UInventorySlotWidget*> ContainerSlotMap;

	UPROPERTY() TArray<UInventoryItemWidget*> PlayerItemWidgets;
	UPROPERTY() TArray<UInventoryItemWidget*> ContainerItemWidgets;

	int32 PlayerHoveredIndex = INDEX_NONE;
	int32 ContainerHoveredIndex = INDEX_NONE;

	UPROPERTY() UItemDragDropOperation* ActiveDragOp = nullptr;
};
