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
	// 마우스 이벤트를 가로채면 그 아래 위젯의 호버/드래그 처리가 끊기므로 HitTestInvisible로 표시한다.
	SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UItemActionTooltipWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GetVisibility() == ESlateVisibility::Collapsed) return;

	UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Slot);
	UPanelWidget* ParentWidget = GetParent();
	if (!CanvasSlot || !ParentWidget) return;

	// 뷰포트 픽셀 좌표를 그대로 캔버스 위치에 넣으면 DPI 스케일/해상도에 따라 커서와 어긋나므로,
	// 부모 캔버스의 지오메트리를 통해 절대 좌표를 로컬 좌표로 정확히 변환한다.
	const FGeometry& ParentGeometry = ParentWidget->GetCachedGeometry();
	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	const FVector2D LocalMousePos = ParentGeometry.AbsoluteToLocal(AbsoluteMousePos);

	CanvasSlot->SetPosition(LocalMousePos + FVector2D(16.f, 16.f));
}
