// 흩어져 있던 디버그 진입점(플레이어 스탯 가감 함수, 동료 명령 함수, companion.* CVar들)을
// 클릭 한 번으로 조작할 수 있게 모아놓은 Slate 디버그 패널. 순수 UI만 담당하고, 실제 어떤 액션이
// 무엇을 하는지는 전혀 모른다 - AProtoCharacter::ToggleDebugPanel()이 FProtoDebugSection 목록을
// 구성해 넘겨준다(패널과 액션 로직을 분리해, 기존 Debug* 함수를 그대로 재사용하면서 진입점만 늘림).
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

// 버튼 하나: 라벨 + 클릭 시 실행할 콜백.
struct FProtoDebugAction
{
	FText Label;
	TFunction<void()> OnClicked;
};

// 슬라이더 하나: 라벨 + 범위 + 현재값 읽기/쓰기 콜백(정규화는 패널 내부에서 처리).
struct FProtoDebugSlider
{
	FText Label;
	float MinValue = 0.0f;
	float MaxValue = 1.0f;
	TFunction<float()> GetValue;
	TFunction<void(float)> SetValue;
};

// 체크박스 하나.
struct FProtoDebugToggle
{
	FText Label;
	TFunction<bool()> GetValue;
	TFunction<void(bool)> SetValue;
};

// 제목 하나 아래 버튼/슬라이더/토글을 순서대로 나열하는 섹션 하나(예: "플레이어 상태", "동료 명령").
struct FProtoDebugSection
{
	FText Title;
	TArray<FProtoDebugAction> Actions;
	TArray<FProtoDebugSlider> Sliders;
	TArray<FProtoDebugToggle> Toggles;
};

class SProtoDebugPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SProtoDebugPanel) {}
		SLATE_ARGUMENT(TArray<FProtoDebugSection>, Sections)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedRef<SWidget> BuildSection(const FProtoDebugSection& Section) const;
};
