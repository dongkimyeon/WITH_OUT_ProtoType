#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "InventoryItemWidget.generated.h"

class UInventoryScreenWidget;
class UInventoryGridComponent;

UCLASS()
class PROTOPROJECT_API UInventoryItemWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitItem(UInventoryScreenWidget* InParentScreen, UInventoryGridComponent* InComponent, int32 InItemIndex);
	void RefreshVisual();
protected:
	UPROPERTY(meta = (BindWidget))
	UImage* ItemImage;

	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	UPROPERTY()
	UInventoryScreenWidget* ParentScreen = nullptr;

	UPROPERTY()
	UInventoryGridComponent* InventoryComponent = nullptr;

	int32 ItemIndex = INDEX_NONE;
};
