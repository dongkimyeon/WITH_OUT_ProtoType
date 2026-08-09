#include "QuickSlotHudWidget.h"
#include "QuickSlotComponent.h"
#include "ItemDataBase.h"
#include "InventoryIconUtils.h"
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

	IconMatInst = FInventoryIconUtils::ApplyIcon(ItemImage, IconBaseMaterial, Entry.ItemData->Icon.LoadSynchronous(), this);
	FInventoryIconUtils::UpdateStackCountText(StackCountText, Entry.ItemData->bIsStackable, Entry.StackCount);
}
