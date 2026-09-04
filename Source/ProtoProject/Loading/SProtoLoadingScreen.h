#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Styling/SlateBrush.h"

class SProtoLoadingScreen : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoLoadingScreen) {}
		SLATE_ARGUMENT(FSlateBrush, BackgroundBrush)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	FSlateBrush BackgroundBrush;
	float FakeProgress = 0.f;
	float CombinedProgress = 0.f;

	TOptional<float> GetProgress() const;
};
