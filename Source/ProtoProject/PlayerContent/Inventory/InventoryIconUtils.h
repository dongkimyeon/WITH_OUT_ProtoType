#pragma once

#include "CoreMinimal.h"

class UImage;
class UTextBlock;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

// 아이콘 표시 공용 유틸
class PROTOPROJECT_API FInventoryIconUtils
{
public:
	// 아이콘 머티리얼 적용
	static UMaterialInstanceDynamic* ApplyIcon(UImage* IconImage, UMaterialInterface* BaseMaterial, UTexture2D* Texture, UObject* Outer);

	// 스택 수량 텍스트 표시
	static void UpdateStackCountText(UTextBlock* StackCountText, bool bIsStackable, int32 StackCount);
};
