#include "DropQuantityPopupWidget.h"
#include "InventoryScreenWidget.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

void UDropQuantityPopupWidget::InitPopup(UInventoryScreenWidget* InParentScreen, UInventoryGridComponent* InInventoryComponent, const FGuid& InInstanceId, int32 InMaxCount)
{
	ParentScreen = InParentScreen;
	InventoryComponent = InInventoryComponent;
	InstanceId = InInstanceId;
	MaxCount = FMath::Max(1, InMaxCount);
	SelectedCount = MaxCount;

	if (QuantitySlider)
	{
		QuantitySlider->SetMinValue(1.f);
		QuantitySlider->SetMaxValue(static_cast<float>(MaxCount));
		QuantitySlider->SetValue(static_cast<float>(SelectedCount));
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(SelectedCount));
	}
}

void UDropQuantityPopupWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (QuantitySlider)
	{
		QuantitySlider->OnValueChanged.AddDynamic(this, &UDropQuantityPopupWidget::HandleSliderValueChanged);
	}
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::HandleConfirmClicked);
	}
	if (CancelButton)
	{
		CancelButton->OnClicked.AddDynamic(this, &UDropQuantityPopupWidget::HandleCancelClicked);
	}
}

void UDropQuantityPopupWidget::HandleSliderValueChanged(float Value)
{
	SelectedCount = FMath::Clamp(FMath::RoundToInt(Value), 1, MaxCount);
	if (QuantityText)
	{
		QuantityText->SetText(FText::AsNumber(SelectedCount));
	}
}

void UDropQuantityPopupWidget::HandleConfirmClicked()
{
	if (ParentScreen && InventoryComponent)
	{
		ParentScreen->PerformPartialDrop(InventoryComponent, InstanceId, SelectedCount);
	}
	RemoveFromParent();
}

void UDropQuantityPopupWidget::HandleCancelClicked()
{
	RemoveFromParent();
}
