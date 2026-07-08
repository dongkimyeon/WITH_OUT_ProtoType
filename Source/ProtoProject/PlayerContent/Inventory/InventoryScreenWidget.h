#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryItemWidget.h"
#include "InventorySlotWidget.h"
#include "InventoryScreenWidget.generated.h"

class UInventoryGridComponent;
class UGridPanel;
class UItemDataBase;
UCLASS()
class PROTOPROJECT_API UInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeGrid(UInventoryGridComponent* InInventoryComponent);

	void OnItemHoverBegin(int32 ItemIndex);
	void OnItemHoverEnd(int32 ItemIndex);
	bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition);
	FVector2D GetCellPixelSize() const { return SlotPixelSize; }
	void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex);
	void ClearDragHighlight();
	
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY(meta = (BindWidget))
	UGridPanel* InventoryGridPanel;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventorySlotWidget> SlotWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UInventoryItemWidget> ItemWidgetClass;

	// WBP_InventorySlot의 슬롯 1칸 픽셀 크기 (WidthOverride/HeightOverride 값과 동일하게 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FVector2D SlotPixelSize = FVector2D(75.f, 75.f);

private:
	void RefreshItemWidget(int32 ItemIndex);

	UPROPERTY()
	UInventoryGridComponent* CachedInventoryComponent = nullptr;

	UPROPERTY()
	TArray<UInventoryItemWidget*> ItemWidgets;

	UPROPERTY()
	TMap<FIntPoint, UInventorySlotWidget*> SlotWidgetMap;
	
	int32 HoveredItemIndex = INDEX_NONE;
};
