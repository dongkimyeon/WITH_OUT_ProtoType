#include "ProtoConnectPrompt.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"

/*-------------------
 위젯 구성
-------------------*/
void SProtoConnectPrompt::Construct(const FArguments& InArgs)
{
	OnConnectRequested = InArgs._OnConnectRequested;

	ChildSlot
	[
		SNew(SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.Padding(24.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(TEXT("서버 IP를 입력하세요")))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SAssignNew(IpTextBox, SEditableTextBox)
					.Text(FText::FromString(InArgs._InitialServerIp))
					.MinDesiredWidth(240.0f)
					.OnTextCommitted(this, &SProtoConnectPrompt::HandleTextCommitted)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.OnClicked(this, &SProtoConnectPrompt::HandleConnectClicked)
					[
						SNew(STextBlock).Text(FText::FromString(TEXT("접속 (Connect)")))
					]
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SAssignNew(StatusTextBlock, STextBlock)
					.Text(FText::GetEmpty())
				]
			]
		]
	];
}

/*-------------------
 이벤트 핸들러
-------------------*/
FReply SProtoConnectPrompt::HandleConnectClicked()
{
	if (IpTextBox.IsValid() && OnConnectRequested.IsBound())
		OnConnectRequested.Execute(IpTextBox->GetText().ToString());

	return FReply::Handled();
}

void SProtoConnectPrompt::HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter)
		HandleConnectClicked();
}

void SProtoConnectPrompt::SetStatusText(const FText& Status)
{
	if (StatusTextBlock.IsValid())
		StatusTextBlock->SetText(Status);
}
