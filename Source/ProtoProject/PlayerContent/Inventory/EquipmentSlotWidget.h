#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EquipmentComponent.h"
#include "EquipmentSlotWidget.generated.h"

class UBorder;
class UInventoryScreenWidget;

// 장착 칸 하나 (헬멧/조끼/무기1/무기2)
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UEquipmentSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(UInventoryScreenWidget* InParentScreen, EEquipmentSlot InSlot, UEquipmentComponent* InEquipmentComponent);
	void RefreshVisual();

protected:
	UPROPERTY(meta = (BindWidget))
	UImage* ItemImage;

	UPROPERTY(meta = (BindWidget))
	UBorder* SlotBorder;

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
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual void NativeOnDragCancelled(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

private:
	EEquipmentSlot Slot = EEquipmentSlot::Helmet;

	UPROPERTY()
	UEquipmentComponent* EquipmentComponentRef = nullptr;

	UPROPERTY()
	UInventoryScreenWidget* ParentScreen = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* IconMatInst = nullptr;
};
