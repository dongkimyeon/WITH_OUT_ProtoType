#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "InventoryScreenBase.h"
#include "InventoryItemWidget.generated.h"

class UInventoryGridComponent;

UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitItem(UInventoryScreenBase* InParentScreen, UInventoryGridComponent* InComponent, int32 InItemIndex);
	void RefreshVisual();
protected:
	UPROPERTY(meta = (BindWidget))
	UImage* ItemImage;

	// 스택 수량 표시
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StackCountText;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	UMaterialInterface* IconBaseMaterial = nullptr;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	UPROPERTY()
	UInventoryScreenBase* ParentScreen = nullptr;

	UPROPERTY()
	UInventoryGridComponent* InventoryComponent = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* IconMatInst = nullptr;

	int32 ItemIndex = INDEX_NONE;

};
