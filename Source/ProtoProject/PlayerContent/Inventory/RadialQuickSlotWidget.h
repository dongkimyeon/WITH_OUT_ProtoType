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

	// 현재 하이라이트된 항목이 가리키는 실제 퀵슬롯 인덱스 (없거나 빈 슬롯이면 INDEX_NONE)
	int32 GetHighlightedSlotIndex() const;

protected:
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// WBP 디자이너에서 배치를 미리 볼 수 있도록, 디자인 타임에는 빈 슬롯 미리보기를 채운다.
	virtual void NativePreConstruct() override;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* RadialCanvas;

	// 디자인 타임 미리보기에 표시할 슬롯 개수 (실제 게임 중에는 QuickSlotComponent->NumSlots를 사용)
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

	// 슬롯별 스택 수량 표시 (스택 가능 아이템이 2개 이상일 때만 보임)
	UPROPERTY()
	TArray<UTextBlock*> EntryStackCountTexts;

	UPROPERTY()
	UQuickSlotComponent* QuickSlotComponentRef = nullptr;

	int32 HighlightedEntryIndex = INDEX_NONE;

	// 원형 배치 한 칸(Border+아이콘+수량 텍스트)을 만들어 RadialCanvas에 추가한다.
	// OpenRadial(실제 데이터)과 NativePreConstruct(디자인 타임 미리보기)가 공용으로 사용.
	void BuildEntry(int32 SlotIndex, int32 IndexInCircle, int32 TotalCount, const FQuickSlotEntry& SlotEntry);
};
