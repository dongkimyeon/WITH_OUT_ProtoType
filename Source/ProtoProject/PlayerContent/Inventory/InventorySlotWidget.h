#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenBase.h"
#include "InventorySlotWidget.generated.h"

class UInventoryGridComponent;
class UBorder;

UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UInventorySlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(UInventoryScreenBase* InParentScreen, FIntPoint InSlotPosition, UInventoryGridComponent* InOwningComponent);
	void SetHighlight(bool bShowHighlight, bool bIsValid);

protected:
	virtual bool NativeOnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	
	UPROPERTY(meta = (BindWidget))
	UBorder* SlotBorder;

	// 색상 설정
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor DefaultColor = FLinearColor(1.f, 1.f, 1.f, 0.05f); // 평상시

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor ValidColor = FLinearColor(0.f, 1.f, 0.f, 0.4f);   // 초록색 (배치 가능)

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor InvalidColor = FLinearColor(1.f, 0.f, 0.f, 0.4f); // 빨간색 (배치 불가)

private:
	UPROPERTY()
	UInventoryScreenBase* ParentScreen = nullptr;

	UPROPERTY()
	UInventoryGridComponent* OwningInventoryComponent = nullptr;

	FIntPoint SlotPosition;
};