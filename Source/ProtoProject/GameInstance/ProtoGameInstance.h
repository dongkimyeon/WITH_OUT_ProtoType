#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Styling/SlateBrush.h"
#include "ProtoGameInstance.generated.h"

UCLASS()
class PROTOPROJECT_API UProtoGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;
	virtual void Shutdown() override;

	// 에디터에서 로딩 배경 이미지 설정 (BP_ProtoGameInstance 디폴트에서 지정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading Screen")
	FSlateBrush LoadingBackgroundBrush;

private:
	void BeginLoadingScreen(const FWorldContext& WorldContext, const FString& MapName);
	void EndLoadingScreen(UWorld* LoadedWorld);
};
