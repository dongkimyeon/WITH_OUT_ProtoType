#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ItemActionTooltipWidget.generated.h"

class UTextBlock;

// 마우스 커서를 따라다니며 "장착"/"사용"/"해제" 같은 우클릭 동작 힌트를 표시하는 툴팁.
// 카테고리별 하드코딩 없이 호출부에서 넘겨준 텍스트를 그대로 표시하기만 한다.
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API UItemActionTooltipWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 빈 텍스트를 넘기면 툴팁을 숨긴다.
	void SetActionText(const FText& Text);

protected:
	UPROPERTY(meta = (BindWidget))
	UTextBlock* ActionText;

	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
};
