#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlayerStatusComponent.h"
#include "PlayerDefalutUI.generated.h"

class UTextBlock;
class UCanvasPanel;
class UProgressBar;
class UQuickSlotHudWidget;
class ACompanionNPC;

UCLASS()
class PROTOPROJECT_API UPlayerDefalutUI : public UUserWidget
{
	GENERATED_BODY()

public:
	void AddInteractPrompt(AActor* Actor, FText Text);
	void RemoveInteractPrompt(AActor* Actor);

	// 동료 머리 위에 이름 + 현재 AI 상태(전투/따라가기/이동/탐색/정지)를 3D->2D 투영해 표시한다
	// (WidgetComponent 없이 AddInteractPrompt와 동일한 방식으로 이 HUD 캔버스 위에 그린다).
	void AddCompanionStatusLabel(ACompanionNPC* Companion);
	void RemoveCompanionStatusLabel(ACompanionNPC* Companion);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float DeltaTime) override;

private:
	UFUNCTION()
	void HandleHealthChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleHungerChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleThirstChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleInfectionChanged(float NewValue, float MaxValue);

	UFUNCTION()
	void HandleStaminaChanged(float NewValue, float MaxValue);

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* PromptCanvas;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HealthProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* HungerProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* ThirstProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* InfectionProgressBar;

	UPROPERTY(meta = (BindWidget))
	UProgressBar* StaminaProgressBar;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HealthText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* HungerText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* ThirstText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* InfectionText;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* StaminaText;

	// 4번 키를 짧게 탭했을 때 바로 사용될 아이템을 우측 하단에 표시
	UPROPERTY(meta = (BindWidgetOptional))
	UQuickSlotHudWidget* QuickSlotHud;

	UPlayerStatusComponent* PlayerStatusComponent;

	TMap<AActor*, UTextBlock*> PromptMap;

	// 동료 머리 위 상태 라벨. 동료가 파괴되는 경우(레벨 전환 등)를 대비해 약참조로 보관한다.
	TMap<TWeakObjectPtr<ACompanionNPC>, UTextBlock*> CompanionStatusMap;

	// 동료 캡슐 기준 상태 라벨을 띄우는 높이.
	UPROPERTY(EditAnywhere, Category = "Companion")
	float CompanionStatusLabelHeight = 220.0f;
};
