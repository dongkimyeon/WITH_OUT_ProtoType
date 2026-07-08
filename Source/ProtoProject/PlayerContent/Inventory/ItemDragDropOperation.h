#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "ItemDragDropOperation.generated.h"

class UItemDataBase;

UCLASS()
class PROTOPROJECT_API UItemDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	// 드래그 중인 아이템 데이터
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	UItemDataBase* DraggedItemData;

	// 원래 있던 자리 좌표 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	FIntPoint OriginalPosition;

	// 원래 회전 상태
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	bool bOriginalRotated;

	// Items 배열 내 인덱스 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop", meta = (ExposeOnSpawn="true"))
	int32 ItemIndex = INDEX_NONE;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Drag and Drop")
	FIntPoint DragOffset = FIntPoint(0, 0);
	
};