#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DropQuantityPopupWidget.generated.h"

class USlider;
class UTextBlock;
class UButton;
class UInventoryScreenWidget;
class UInventoryGridComponent;

// 수량 선택 팝업
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UDropQuantityPopupWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitPopup(UInventoryScreenWidget* InParentScreen, UInventoryGridComponent* InInventoryComponent, const FGuid& InInstanceId, int32 InMaxCount);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(meta = (BindWidget))
	USlider* QuantitySlider;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* QuantityText;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmButton;

	UPROPERTY(meta = (BindWidget))
	UButton* CancelButton;

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleCancelClicked();

private:
	UPROPERTY()
	UInventoryScreenWidget* ParentScreen = nullptr;

	UPROPERTY()
	UInventoryGridComponent* InventoryComponent = nullptr;

	FGuid InstanceId;
	int32 MaxCount = 1;
	int32 SelectedCount = 1;
};
