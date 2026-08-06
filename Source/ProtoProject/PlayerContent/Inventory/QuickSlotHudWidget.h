#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "QuickSlotHudWidget.generated.h"

class AProtoCharacter;
class UQuickSlotComponent;

// HUD 상시 표시용 위젯. QuickSlotComponent::LastUsedSlotIndex(4번 키를 짧게 탭했을 때 바로 사용될 슬롯)의
// 아이템 아이콘을 보여준다. 등록/해제/사용/스왑 등 어떤 변화가 생겨도 QuickSlotComponent가 브로드캐스트하는
// OnQuickSlotChanged 델리게이트 하나만 구독해서 매번 LastUsedSlotIndex 기준으로 다시 그린다.
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
