#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "QuickSlotComponent.h"
#include "QuickSlotRegisterWidget.generated.h"

class UBorder;
class UInventoryScreenWidget;

// 퀵슬롯 등록 칸 하나
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UQuickSlotRegisterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(UInventoryScreenWidget* InParentScreen, int32 InSlotIndex, UQuickSlotComponent* InQuickSlotComponent);
	void RefreshVisual();

protected:
	UPROPERTY(meta = (BindWidget))
	UImage* ItemImage;

	UPROPERTY(meta = (BindWidget))
	UBorder* SlotBorder;

	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StackCountText;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	UMaterialInterface* IconBaseMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor DefaultColor = FLinearColor(1.f, 1.f, 1.f, 0.05f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor ValidColor = FLinearColor(0.f, 1.f, 0.f, 0.4f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor InvalidColor = FLinearColor(1.f, 0.f, 0.f, 0.4f);

	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

private:
	int32 SlotIndex = 0;

	UPROPERTY()
	UQuickSlotComponent* QuickSlotComponentRef = nullptr;

	UPROPERTY()
	UInventoryScreenWidget* ParentScreen = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* IconMatInst = nullptr;
};
