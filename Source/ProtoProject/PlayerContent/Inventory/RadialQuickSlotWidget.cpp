#include "RadialQuickSlotWidget.h"
#include "QuickSlotComponent.h"
#include "ItemDataBase.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"

void URadialQuickSlotWidget::OpenRadial(UQuickSlotComponent* InQuickSlotComponent)
{
	QuickSlotComponentRef = InQuickSlotComponent;
	Entries.Empty();
	EntryBorders.Empty();
	HighlightedEntryIndex = INDEX_NONE;

	if (!RadialCanvas || !QuickSlotComponentRef) return;

	RadialCanvas->ClearChildren();

	TArray<int32> OccupiedIndices;
	for (int32 i = 0; i < QuickSlotComponentRef->NumSlots; ++i)
	{
		if (QuickSlotComponentRef->GetQuickSlotEntry(i).ItemData)
		{
			OccupiedIndices.Add(i);
		}
	}

	const int32 Count = OccupiedIndices.Num();
	for (int32 k = 0; k < Count; ++k)
	{
		const int32 SlotIndex = OccupiedIndices[k];
		const FQuickSlotEntry& SlotEntry = QuickSlotComponentRef->GetQuickSlotEntry(SlotIndex);

		// 12시 방향부터 시계방향으로 균등 배치
		const float Angle = ((2.f * PI * k) / Count) - (PI * 0.5f);

		FRadialEntryInfo Info;
		Info.SlotIndex = SlotIndex;
		Info.AngleRad = Angle;
		Entries.Add(Info);

		UBorder* EntryBorder = NewObject<UBorder>(this);
		EntryBorder->SetBrushColor(DefaultEntryColor);

		UImage* Icon = NewObject<UImage>(this);
		if (SlotEntry.ItemData)
		{
			if (UTexture2D* Texture = SlotEntry.ItemData->Icon.LoadSynchronous())
			{
				if (IconBaseMaterial)
				{
					UMaterialInstanceDynamic* MatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, Icon);
					MatInst->SetTextureParameterValue(FName("image"), Texture);
					Icon->SetBrushFromMaterial(MatInst);
				}
			}
		}
		EntryBorder->AddChild(Icon);
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
}

int32 URadialQuickSlotWidget::GetHighlightedSlotIndex() const
{
	return Entries.IsValidIndex(HighlightedEntryIndex) ? Entries[HighlightedEntryIndex].SlotIndex : INDEX_NONE;
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
