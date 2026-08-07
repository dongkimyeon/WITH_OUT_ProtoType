#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;

DECLARE_DELEGATE_OneParam(FProtoOnConnectRequested, const FString& /*ServerIp*/);

// Tiny on-screen prompt: an IP text box + Connect button. Purely
// presentational; UProtoNetClientSubsystem owns the actual Connect() call.
class SProtoConnectPrompt : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoConnectPrompt) {}
		SLATE_ARGUMENT(FString, InitialServerIp)
		SLATE_EVENT(FProtoOnConnectRequested, OnConnectRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetStatusText(const FText& Status);

private:
	FReply HandleConnectClicked();
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	TSharedPtr<SEditableTextBox> IpTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
	FProtoOnConnectRequested OnConnectRequested;
};
