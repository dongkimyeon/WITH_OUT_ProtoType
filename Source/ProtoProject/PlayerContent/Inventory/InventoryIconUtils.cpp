#include "InventoryIconUtils.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Materials/MaterialInstanceDynamic.h"

UMaterialInstanceDynamic* FInventoryIconUtils::ApplyIcon(UImage* IconImage, UMaterialInterface* BaseMaterial, UTexture2D* Texture, UObject* Outer)
{
	if (!IconImage) return nullptr;

	if (Texture && BaseMaterial)
	{
		UMaterialInstanceDynamic* MatInst = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
		MatInst->SetTextureParameterValue(FName("image"), Texture);
		IconImage->SetBrushFromMaterial(MatInst);
		IconImage->SetVisibility(ESlateVisibility::Visible);
		return MatInst;
	}

	// 텍스처 없으면 숨김
	IconImage->SetVisibility(ESlateVisibility::Hidden);
	return nullptr;
}

void FInventoryIconUtils::UpdateStackCountText(UTextBlock* StackCountText, bool bIsStackable, int32 StackCount)
{
	if (!StackCountText) return;

	if (bIsStackable && StackCount > 1)
	{
		StackCountText->SetText(FText::AsNumber(StackCount));
		StackCountText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	else
	{
		StackCountText->SetVisibility(ESlateVisibility::Collapsed);
	}
}
