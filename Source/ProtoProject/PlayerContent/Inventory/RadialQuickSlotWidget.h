#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadialQuickSlotWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UQuickSlotComponent;

// 4번 키를 꾹 눌렀을 때 뜨는 원형 선택 UI. 등록된(점유된) 퀵슬롯 칸들만 원형으로 배치하고,
// 마우스를 화면 중앙에서 바깥으로 움직인 방향에 따라 실시간으로 가장 가까운 항목을 하이라이트한다.
UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API URadialQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 점유된 슬롯들을 원형으로 배치해서 연다.
	void OpenRadial(UQuickSlotComponent* InQuickSlotComponent);

	// 현재 하이라이트된 항목이 가리키는 실제 퀵슬롯 인덱스 (없으면 INDEX_NONE)
	int32 GetHighlightedSlotIndex() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* RadialCanvas;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	UMaterialInterface* IconBaseMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float RadiusPixels = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float EntrySize = 72.f;

	// 마우스가 중앙에서 이 반경 안에 있으면 아무 것도 하이라이트하지 않는다 (오조작 방지).
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float DeadZoneRadiusPixels = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor DefaultEntryColor = FLinearColor(1.f, 1.f, 1.f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor HighlightEntryColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);

private:
	// 반사(리플렉션) 대상이 아닌 순수 데이터 - 실제 위젯 포인터는 EntryBorders에서 GC 안전하게 관리한다.
	struct FRadialEntryInfo
	{
		int32 SlotIndex = INDEX_NONE;
		float AngleRad = 0.f;
	};
	TArray<FRadialEntryInfo> Entries;

	UPROPERTY()
	TArray<UBorder*> EntryBorders;

	UPROPERTY()
	UQuickSlotComponent* QuickSlotComponentRef = nullptr;

	int32 HighlightedEntryIndex = INDEX_NONE;
};
