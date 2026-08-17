#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemActionTooltipWidget.generated.h"

class UTextBlock;

// 마우스 추적 액션 툴팁
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UItemActionTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetActionText(const FText& Text);
protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionText;
	
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
