#include "QuickSlotHudWidget.h"
#include "QuickSlotComponent.h"
#include "ItemDataBase.h"
#include "../ProtoCharacter.h"
#include "Engine/Texture2D.h"

void UQuickSlotHudWidget::Init(AProtoCharacter* OwningCharacter)
{
	if (!OwningCharacter) return;

	QuickSlotComponentRef = OwningCharacter->GetQuickSlotComponent();
	if (QuickSlotComponentRef)
	{
		QuickSlotComponentRef->OnQuickSlotChanged.AddDynamic(this, &UQuickSlotHudWidget::HandleQuickSlotChanged);
	}

	RefreshFromLastUsedSlot();
}

void UQuickSlotHudWidget::HandleQuickSlotChanged(int32 SlotIndex)
{
	RefreshFromLastUsedSlot();
}

void UQuickSlotHudWidget::RefreshFromLastUsedSlot()
{
	if (!ItemImage) return;

	if (!QuickSlotComponentRef || QuickSlotComponentRef->LastUsedSlotIndex == INDEX_NONE)
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
		if (StackCountText) StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const FQuickSlotEntry& Entry = QuickSlotComponentRef->GetQuickSlotEntry(QuickSlotComponentRef->LastUsedSlotIndex);
	if (!Entry.ItemData)
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
		if (StackCountText) StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	UTexture2D* Texture = Entry.ItemData->Icon.LoadSynchronous();
	if (Texture && IconBaseMaterial)
	{
		IconMatInst = UMaterialInstanceDynamic::Create(IconBaseMaterial, this);
		IconMatInst->SetTextureParameterValue(FName("image"), Texture);
		ItemImage->SetBrushFromMaterial(IconMatInst);
		ItemImage->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		ItemImage->SetVisibility(ESlateVisibility::Hidden);
	}

	if (StackCountText)
	{
		if (Entry.ItemData->bIsStackable && Entry.StackCount > 1)
		{
			StackCountText->SetText(FText::AsNumber(Entry.StackCount));
			StackCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			StackCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
