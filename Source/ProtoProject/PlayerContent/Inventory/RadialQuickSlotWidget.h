#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RadialQuickSlotWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UTextBlock;
class UQuickSlotComponent;
struct FQuickSlotEntry;

UCLASS(meta = (PrioritizeCategories = "Inventory UI"))
class PROTOPROJECT_API URadialQuickSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void OpenRadial(UQuickSlotComponent* InQuickSlotComponent);

	// 하이라이트된 슬롯 인덱스 조회
	int32 GetHighlightedSlotIndex() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 디자이너 미리보기
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* RadialCanvas;

	// 미리보기 슬롯 개수
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	int32 PreviewSlotCount = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	UMaterialInterface* IconBaseMaterial = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float RadiusPixels = 140.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float EntrySize = 72.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	float DeadZoneRadiusPixels = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor DefaultEntryColor = FLinearColor(1.f, 1.f, 1.f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	FLinearColor HighlightEntryColor = FLinearColor(1.f, 1.f, 1.f, 0.6f);

private:
	struct FRadialEntryInfo
	{
		int32 SlotIndex = INDEX_NONE;
		float AngleRad = 0.f;
	};
	TArray<FRadialEntryInfo> Entries;

	UPROPERTY()
	TArray<UBorder*> EntryBorders;

	// 슬롯별 수량 텍스트
	UPROPERTY()
	TArray<UTextBlock*> EntryStackCountTexts;

	UPROPERTY()
	UQuickSlotComponent* QuickSlotComponentRef = nullptr;

	int32 HighlightedEntryIndex = INDEX_NONE;

	// 원형 슬롯 한 칸 생성
	void BuildEntry(int32 SlotIndex, int32 IndexInCircle, int32 TotalCount, const FQuickSlotEntry& SlotEntry);
};
