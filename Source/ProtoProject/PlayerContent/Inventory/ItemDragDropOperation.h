#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ItemDragDropOperation.generated.h"

class UItemDataBase;

UCLASS()
class PROTOPROJECT_API UItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	UItemDataBase* DraggedItemData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	FIntPoint OriginalPosition;

	// 취소 시 복원용 — 드래그 시작 시점의 회전 상태
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	bool bOriginalRotated;

	// 드래그 중 현재 회전 상태 (R키로 변경 가능)
	UPROPERTY()
	bool bCurrentRotated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	int32 ItemIndex = INDEX_NONE;

	UPROPERTY()
	FIntPoint DragOffset = FIntPoint(0, 0);

	// DragVisual 업데이트용 참조
	UPROPERTY()
	UImage* DragVisualImage = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DragVisualMatInst = nullptr;

	UPROPERTY()
	USizeBox* DragVisualWrapper = nullptr;

	FVector2D CellPixelSize = FVector2D(75.f, 75.f);
};