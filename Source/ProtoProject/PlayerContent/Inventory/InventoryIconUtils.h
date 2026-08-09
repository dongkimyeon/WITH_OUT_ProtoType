#pragma once

#include "CoreMinimal.h"

class UImage;
class UTextBlock;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2D;

// 인벤토리/장착/퀵슬롯 위젯들이 공통으로 쓰는 아이콘 표시 로직 (여러 위젯에 복붙돼있던 걸 한 곳으로 모음).
class PROTOPROJECT_API FInventoryIconUtils
{
public:
	// 아이콘 이미지에 텍스처를 입힌 동적 머티리얼을 만들어 적용한다. 텍스처/베이스 머티리얼이 없으면 아이콘을 숨긴다.
	// 반환된 MID는 회전 등 위젯별로 다른 추가 파라미터를 세팅해야 할 때 호출부에서 계속 사용할 수 있다.
	static UMaterialInstanceDynamic* ApplyIcon(UImage* IconImage, UMaterialInterface* BaseMaterial, UTexture2D* Texture, UObject* Outer);

	// 스택 가능한 아이템이 2개 이상일 때만 수량 텍스트를 보여준다.
	static void UpdateStackCountText(UTextBlock* StackCountText, bool bIsStackable, int32 StackCount);
};
