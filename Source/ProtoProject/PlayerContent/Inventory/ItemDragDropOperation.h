#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EquipmentComponent.h"
#include "ItemDragDropOperation.generated.h"

class UItemDataBase;
class UInventoryGridComponent;
class UInventoryScreenBase;

UCLASS(meta = (PrioritizeCategories = "Drag and Drop"))
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

	// 인덱스가 shift되어도 안정적으로 원본 아이템을 가리키기 위한 고유 ID (장착/퀵슬롯 드롭 대상에서 사용)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	FGuid InstanceId;

	UPROPERTY()
	FIntPoint DragOffset = FIntPoint(0, 0);
\
	UPROPERTY()
	UImage* DragVisualImage = nullptr;

	UPROPERTY()
	UMaterialInstanceDynamic* DragVisualMatInst = nullptr;

	UPROPERTY()
	USizeBox* DragVisualWrapper = nullptr;

	FVector2D CellPixelSize = FVector2D(75.f, 75.f);
\
	UPROPERTY()
	UInventoryGridComponent* SourceInventoryComponent = nullptr;

	UPROPERTY()
	UInventoryScreenBase* SourceScreenWidget = nullptr;

	// 장착 슬롯에서 시작된 드래그일 경우에만 유효 (그리드에서 시작된 드래그는 SourceInventoryComponent를 사용)
	UPROPERTY()
	UEquipmentComponent* SourceEquipmentComponent = nullptr;

	UPROPERTY()
	EEquipmentSlot SourceEquipmentSlot = EEquipmentSlot::Helmet;
};