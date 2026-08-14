#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SEditableTextBox;
class STextBlock;

DECLARE_DELEGATE_ThreeParams(FProtoOnConnectRequested, const FString& /*ServerIp*/, const FString& /*Username*/, const FString& /*Password*/);

// Tiny on-screen prompt: server IP + account username/password + Connect
// button. Purely presentational; UProtoNetClientSubsystem owns the actual
// Connect()/login call. A blank/"0" username means "skip logging into an
// account" (see UProtoNetClientSubsystem::HandleConnectPromptSubmitted) --
// existing test/offline flows that don't care about accounts still work.
class SProtoConnectPrompt : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoConnectPrompt) {}
		SLATE_ARGUMENT(FString, InitialServerIp)
		SLATE_ARGUMENT(FString, InitialUsername)
		SLATE_EVENT(FProtoOnConnectRequested, OnConnectRequested)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetStatusText(const FText& Status);

private:
	FReply HandleConnectClicked();
	void HandleTextCommitted(const FText& NewText, ETextCommit::Type CommitType);

	TSharedPtr<SEditableTextBox> IpTextBox;
	TSharedPtr<SEditableTextBox> UsernameTextBox;
	TSharedPtr<SEditableTextBox> PasswordTextBox;
	TSharedPtr<STextBlock> StatusTextBlock;
	FProtoOnConnectRequested OnConnectRequested;
};
