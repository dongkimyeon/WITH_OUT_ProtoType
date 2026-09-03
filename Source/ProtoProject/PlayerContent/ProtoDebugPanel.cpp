#include "ProtoDebugPanel.h"

#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Styling/CoreStyle.h"

void SProtoDebugPanel::Construct(const FArguments& InArgs)
{
	const TArray<FProtoDebugSection>& Sections = InArgs._Sections;

	TSharedRef<SVerticalBox> SectionsBox = SNew(SVerticalBox);
	for (const FProtoDebugSection& Section : Sections)
	{
		SectionsBox->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 10.0f)
			[
				BuildSection(Section)
			];
	}

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Top)
	[
		SNew(SBox)
		.WidthOverride(340.0f)
		.MaxDesiredHeight(800.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(FLinearColor(0.02f, 0.02f, 0.02f, 0.9f))
			.Padding(12.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SectionsBox
				]
			]
		]
	];
}

TSharedRef<SWidget> SProtoDebugPanel::BuildSection(const FProtoDebugSection& Section) const
{
	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	if (Section.Actions.Num() > 0)
	{
		TSharedRef<SWrapBox> ButtonWrap = SNew(SWrapBox).UseAllottedSize(true);
		for (const FProtoDebugAction& Action : Section.Actions)
		{
			TFunction<void()> Callback = Action.OnClicked;
			ButtonWrap->AddSlot()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(Action.Label)
					.OnClicked_Lambda([Callback]() -> FReply
					{
						if (Callback)
						{
							Callback();
						}
						return FReply::Handled();
					})
				];
		}

		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 2.0f)
			[
				ButtonWrap
			];
	}

	for (const FProtoDebugSlider& Slider : Section.Sliders)
	{
		TFunction<float()> Getter = Slider.GetValue;
		TFunction<void(float)> Setter = Slider.SetValue;
		const float MinV = Slider.MinValue;
		const float MaxV = Slider.MaxValue;

		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(Slider.Label).MinDesiredWidth(120.0f)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(6.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SSlider)
					.Value_Lambda([Getter, MinV, MaxV]() -> float
					{
						const float V = Getter ? Getter() : MinV;
						return (MaxV > MinV) ? (V - MinV) / (MaxV - MinV) : 0.0f;
					})
					.OnValueChanged_Lambda([Setter, MinV, MaxV](float Normalized)
					{
						if (Setter)
						{
							Setter(MinV + Normalized * (MaxV - MinV));
						}
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock).Text_Lambda([Getter]()
					{
						return FText::AsNumber(Getter ? Getter() : 0.0f);
					})
				]
			];
	}

	for (const FProtoDebugToggle& Toggle : Section.Toggles)
	{
		TFunction<bool()> Getter = Toggle.GetValue;
		TFunction<void(bool)> Setter = Toggle.SetValue;

		Body->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Getter]()
				{
					return (Getter && Getter()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([Setter](ECheckBoxState NewState)
				{
					if (Setter)
					{
						Setter(NewState == ECheckBoxState::Checked);
					}
				})
				[
					SNew(STextBlock).Text(Toggle.Label)
				]
			];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(Section.Title)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			Body
		];
}
