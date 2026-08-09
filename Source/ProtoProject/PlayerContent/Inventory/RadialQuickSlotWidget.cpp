#include "RadialQuickSlotWidget.h"
#include "QuickSlotComponent.h"
#include "ItemDataBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InventoryIconUtils.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

void URadialQuickSlotWidget::OpenRadial(UQuickSlotComponent* InQuickSlotComponent)
{
	QuickSlotComponentRef = InQuickSlotComponent;
	Entries.Empty();
	EntryBorders.Empty();
	EntryStackCountTexts.Empty();
	HighlightedEntryIndex = INDEX_NONE;

	if (!RadialCanvas || !QuickSlotComponentRef) return;

	RadialCanvas->ClearChildren();

	const int32 Count = QuickSlotComponentRef->NumSlots;
	for (int32 k = 0; k < Count; ++k)
	{
		BuildEntry(k, k, Count, QuickSlotComponentRef->GetQuickSlotEntry(k));
	}
}

void URadialQuickSlotWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	// 실제 플레이 중엔 OpenRadial()이 매번 다시 채우므로, 여기서는 에디터 디자이너 미리보기만 처리한다.
	if (!IsDesignTime() || !RadialCanvas) return;

	RadialCanvas->ClearChildren();
	Entries.Empty();
	EntryBorders.Empty();
	EntryStackCountTexts.Empty();

	const FQuickSlotEntry EmptyPreviewEntry;
	for (int32 k = 0; k < PreviewSlotCount; ++k)
	{
		BuildEntry(k, k, PreviewSlotCount, EmptyPreviewEntry);
	}
}

void URadialQuickSlotWidget::BuildEntry(int32 SlotIndex, int32 IndexInCircle, int32 TotalCount, const FQuickSlotEntry& SlotEntry)
{
	// 12시 방향부터 시계방향으로 균등 배치
	const float Angle = ((2.f * PI * IndexInCircle) / TotalCount) - (PI * 0.5f);

	FRadialEntryInfo Info;
	Info.SlotIndex = SlotIndex;
	Info.AngleRad = Angle;
	Entries.Add(Info);

	UBorder* EntryBorder = NewObject<UBorder>(this);
	EntryBorder->SetBrushColor(DefaultEntryColor);

	UImage* Icon = NewObject<UImage>(this);
	FInventoryIconUtils::ApplyIcon(Icon, IconBaseMaterial, SlotEntry.ItemData ? SlotEntry.ItemData->Icon.LoadSynchronous() : nullptr, Icon);

	UTextBlock* StackCountText = NewObject<UTextBlock>(this);
	FInventoryIconUtils::UpdateStackCountText(StackCountText, SlotEntry.ItemData && SlotEntry.ItemData->bIsStackable, SlotEntry.StackCount);
	EntryStackCountTexts.Add(StackCountText);

	// UBorder는 자식을 하나만 가질 수 있으므로 Overlay로 아이콘 위에 수량 텍스트를 얹는다.
	UOverlay* EntryOverlay = NewObject<UOverlay>(this);
	if (UOverlaySlot* IconSlot = EntryOverlay->AddChildToOverlay(Icon))
	{
		IconSlot->SetHorizontalAlignment(HAlign_Fill);
		IconSlot->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* StackSlot = EntryOverlay->AddChildToOverlay(StackCountText))
	{
		StackSlot->SetHorizontalAlignment(HAlign_Right);
		StackSlot->SetVerticalAlignment(VAlign_Bottom);
	}
	EntryBorder->AddChild(EntryOverlay);
	EntryBorders.Add(EntryBorder);

	if (UCanvasPanelSlot* CanvasSlot = RadialCanvas->AddChildToCanvas(EntryBorder))
	{
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f));
		CanvasSlot->SetSize(FVector2D(EntrySize, EntrySize));

		const FVector2D Offset(FMath::Cos(Angle) * RadiusPixels, FMath::Sin(Angle) * RadiusPixels);
		CanvasSlot->SetPosition(Offset - FVector2D(EntrySize * 0.5f, EntrySize * 0.5f));
	}
}

int32 URadialQuickSlotWidget::GetHighlightedSlotIndex() const
{
	if (!Entries.IsValidIndex(HighlightedEntryIndex) || !QuickSlotComponentRef) return INDEX_NONE;

	const int32 SlotIndex = Entries[HighlightedEntryIndex].SlotIndex;
	// 빈 슬롯을 가리키고 있으면 선택 안 한 것으로 취급 (등록되지 않은 칸은 사용/선택 불가)
	return QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex).ItemData ? SlotIndex : INDEX_NONE;
}

void URadialQuickSlotWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (Entries.Num() == 0 || !RadialCanvas) return;

	const FGeometry& CanvasGeometry = RadialCanvas->GetCachedGeometry();
	const FVector2D Center = CanvasGeometry.GetLocalSize() * 0.5f;

	const FVector2D AbsoluteMousePos = FSlateApplication::Get().GetCursorPos();
	const FVector2D LocalMousePos = CanvasGeometry.AbsoluteToLocal(AbsoluteMousePos);
	const FVector2D Dir = LocalMousePos - Center;

	int32 NewHighlight = INDEX_NONE;
	if (Dir.SizeSquared() > DeadZoneRadiusPixels * DeadZoneRadiusPixels)
	{
		const float MouseAngle = FMath::Atan2(Dir.Y, Dir.X);
		float BestDiff = TNumericLimits<float>::Max();
		for (int32 i = 0; i < Entries.Num(); ++i)
		{
			const float Diff = FMath::Abs(FMath::FindDeltaAngleRadians(MouseAngle, Entries[i].AngleRad));
			if (Diff < BestDiff)
			{
				BestDiff = Diff;
				NewHighlight = i;
			}
		}
	}

	if (NewHighlight != HighlightedEntryIndex)
	{
		HighlightedEntryIndex = NewHighlight;
		for (int32 i = 0; i < EntryBorders.Num(); ++i)
		{
			if (EntryBorders[i])
			{
				EntryBorders[i]->SetBrushColor(i == HighlightedEntryIndex ? HighlightEntryColor : DefaultEntryColor);
			}
		}
	}
}
