#pragma once

#include "CoreMinimal.h"
#include "InventoryScreenBase.h"
#include "InventoryItemWidget.h"
#include "InventorySlotWidget.h"
#include "ItemDragDropOperation.h"
#include "InventoryGridComponent.h"
#include "EquipmentComponent.h"
#include "EquipmentSlotWidget.h"
#include "ItemActionTooltipWidget.h"
#include "QuickSlotComponent.h"
#include "QuickSlotRegisterWidget.h"
#include "DropQuantityPopupWidget.h"
#include "InventoryScreenWidget.generated.h"

class UGridPanel;
class UItemDataBase;
class UItemDragDropOperation;
class ADropItem;

UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UInventoryScreenWidget : public UInventoryScreenBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeGrid(UInventoryGridComponent* InInventoryComponent);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeEquipment(UEquipmentComponent* InEquipmentComponent);

	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeQuickSlots(UQuickSlotComponent* InQuickSlotComponent);

	virtual void NativePreConstruct() override;

	virtual FVector2D GetCellPixelSize() const override { return SlotPixelSize; }
	virtual void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent) override;
	virtual void ClearDragHighlight() override;
	virtual bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual bool OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) override;
	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void OnItemContextAction(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void OnItemRequestPartialDrop(int32 ItemIndex, UInventoryGridComponent* OwningComponent) override;
	virtual void RefreshEquipmentSlots() override;
	virtual void RefreshGrid(UInventoryGridComponent* Component) override;
	virtual void RefreshSingleItem(UInventoryGridComponent* Component, int32 ItemIndex, const FGuid& ExpectedInstanceId) override;
	virtual void RefreshQuickSlots() override;

	UInventoryGridComponent* GetCachedInventoryComponent() const { return CachedInventoryComponent; }
	UEquipmentComponent* GetCachedEquipmentComponent() const { return CachedEquipmentComponent; }

	// 액션 툴팁 표시/숨김
	void ShowActionTooltip(const FText& Text);
	void HideActionTooltip();

	// 수량 지정 버리기 실행
	void PerformPartialDrop(UInventoryGridComponent* SourceInventory, const FGuid& InstanceId, int32 Count);

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	// 화면 바깥 드롭 = 월드에 버리기
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<ADropItem> DropItemActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UDropQuantityPopupWidget> DropQuantityPopupClass;

	UPROPERTY(meta = (BindWidget))
	UGridPanel* InventoryGridPanel;

	UPROPERTY(meta = (BindWidgetOptional))
	UEquipmentSlotWidget* HelmetSlotWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	UEquipmentSlotWidget* VestSlotWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	UEquipmentSlotWidget* Weapon1SlotWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	UEquipmentSlotWidget* Weapon2SlotWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	UItemActionTooltipWidget* ActionTooltipWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget0;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget1;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget2;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget3;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget4;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget5;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget6;
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotRegisterWidget* QuickSlotWidget7;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventoryItemWidget> ItemWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FVector2D SlotPixelSize = FVector2D(100.f, 100.f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventoryGridComponent> PreviewGridComponentClass;

private:
	void DropItemToWorld(UItemDragDropOperation* DragOp);
	void SpawnDropItemActor(UItemDataBase* ItemData, int32 StackCount);

	UPROPERTY()
	UInventoryGridComponent* CachedInventoryComponent = nullptr;

	UPROPERTY()
	UEquipmentComponent* CachedEquipmentComponent = nullptr;

	UPROPERTY()
	TArray<UInventoryItemWidget*> ItemWidgets;

	UPROPERTY()
	TMap<FIntPoint, UInventorySlotWidget*> SlotWidgetMap;

	int32 HoveredItemIndex = INDEX_NONE;
};
