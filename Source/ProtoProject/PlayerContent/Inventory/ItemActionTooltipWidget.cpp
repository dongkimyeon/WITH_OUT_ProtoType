#include "ItemActionTooltipWidget.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"

void UItemActionTooltipWidget::SetActionText(const FText& Text)
{
	if (!ActionText) return;

	if (Text.IsEmpty())
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ActionText->SetText(Text);
	// 마우스 이벤트 통과
	SetVisibility(ESlateVisibility::HitTestInvisible);
}


void UItemActionTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() == ESlateVisibility::Collapsed) return;

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	UPanelWidget* ParentWidget = GetParent();
	if (!CanvasSlot || !ParentWidget) return;

	// 마우스 좌표 → 캔버스 로컬 좌표 변환
	const FGeometry& ParentGeometry = ParentWidget->GetCachedGeometry();
	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	const FVector2D LocalMousePos = ParentGeometry.AbsoluteToLocal(AbsoluteMousePos);

	CanvasSlot->SetPosition(LocalMousePos + FVector2D(16.f, 16.f));
}
