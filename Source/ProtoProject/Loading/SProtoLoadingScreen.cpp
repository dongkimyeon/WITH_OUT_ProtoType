#include "SProtoLoadingScreen.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SVerticalBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SOverlay.h"
#include "ShaderPipelineCache.h"
#include "Math/UnrealMathUtility.h"

void SProtoLoadingScreen::Construct(const FArguments& InArgs)
{
	BackgroundBrush = InArgs._BackgroundBrush;

	ChildSlot
	[
		SNew(SOverlay)

		// 배경 이미지
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SNew(SImage)
			.Image(&BackgroundBrush)
		]

		// 하단 진행률 바 영역
		+ SOverlay::Slot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(80.f, 0.f, 80.f, 60.f))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 0.f, 0.f, 8.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("로딩 중...")))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.85f)))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SProgressBar)
				.Percent(TAttribute<TOptional<float>>::Create(
					TAttribute<TOptional<float>>::FGetter::CreateSP(this, &SProtoLoadingScreen::GetProgress)))
				.FillColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.7f, 1.f)))
			]
		]
	];
}

void SProtoLoadingScreen::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// 0 ~ 85% 구간: 시간 기반 Fake 보간
	// OpenLevel은 동기 로드라 GetAsyncLoadPercentage()가 유효한 값을 주지 않으므로
	// 부드럽게 차오르는 가짜 진행률로 대체한다.
	FakeProgress = FMath::FInterpTo(FakeProgress, 0.85f, InDeltaTime, 0.4f);

	// 85 ~ 100% 구간: 셰이더 파이프라인 캐시 컴파일 진행률 (스레드 세이프)
	int32 Remaining = 0, Total = 0;
	FShaderPipelineCache::GetPrecompileProgress(Remaining, Total);
	const float ShaderPct = (Total > 0) ? (1.f - static_cast<float>(Remaining) / static_cast<float>(Total)) : 1.f;

	CombinedProgress = FakeProgress * 0.7f + ShaderPct * 0.3f;
}

TOptional<float> SProtoLoadingScreen::GetProgress() const
{
	return TOptional<float>(CombinedProgress);
}
