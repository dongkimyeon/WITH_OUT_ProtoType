#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "QuickSlotHudWidget.generated.h"

class AProtoCharacter;
class UQuickSlotComponent;

// HUD 상시 표시용 위젯
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UQuickSlotHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void Init(AProtoCharacter* OwningCharacter);

protected:
	UPROPERTY(meta = (BindWidget))
	UImage* ItemImage;

	// 겹칠 수 있는(스택 가능) 아이템의 현재 수량 표시. 수량이 1개뿐이면 숨긴다.
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* StackCountText;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	UMaterialInterface* IconBaseMaterial = nullptr;

	UFUNCTION()
	void HandleQuickSlotChanged(int32 SlotIndex);

private:
	void RefreshFromLastUsedSlot();

	UPROPERTY()
	UQuickSlotComponent* QuickSlotComponentRef = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* IconMatInst = nullptr;
};
