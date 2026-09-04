#include "ProtoGameInstance.h"
#include "../Loading/SProtoLoadingScreen.h"
#include "MoviePlayer.h"
#include "Misc/CoreDelegates.h"

void UProtoGameInstance::Init()
{
	Super::Init();

	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &UProtoGameInstance::BeginLoadingScreen);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UProtoGameInstance::EndLoadingScreen);
}

void UProtoGameInstance::Shutdown()
{
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);

	Super::Shutdown();
}

void UProtoGameInstance::BeginLoadingScreen(const FWorldContext& WorldContext, const FString& MapName)
{
	// 타이틀 레벨(진입 시)에는 로딩화면 표시 안 함
	if (MapName.Contains(TEXT("TitleLevel")) || MapName.Contains(TEXT("Entry")))
	{
		return;
	}

	// 배경 브러시를 값으로 복사해 Slate 위젯에 전달한다.
	// Movie Player 위젯은 별도 렌더 스레드에서 틱 되므로 UObject 참조나
	// 전역 포인터를 위젯 내부에서 접근하면 크래시가 발생한다.
	FSlateBrush BrushCopy = LoadingBackgroundBrush;

	FLoadingScreenAttributes Attrs;
	Attrs.bAutoCompleteWhenLoadingCompletes = true;
	Attrs.MinimumLoadingScreenDisplayTime = 1.5f;
	Attrs.WidgetLoadingScreen = SNew(SProtoLoadingScreen)
		.BackgroundBrush(BrushCopy);

	GetMoviePlayer()->SetupLoadingScreen(Attrs);
}

void UProtoGameInstance::EndLoadingScreen(UWorld* LoadedWorld)
{
	// 로딩 완료 후 추가 처리가 필요하면 여기서 수행
}
