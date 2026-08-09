#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenBase.generated.h"

class UItemDataBase;
class UInventoryGridComponent;
class UItemDragDropOperation;
class UGridPanel;
class UInventorySlotWidget;
class UInventoryItemWidget;

UCLASS(Abstract)
class PROTOPROJECT_API UInventoryScreenBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual FVector2D GetCellPixelSize() const { return FVector2D(75.f, 75.f); }

	virtual void UpdateDragHighlight(const FIntPoint& TargetTopLeft, UItemDataBase* ItemData, bool bRotated, int32 IgnoreIndex, UInventoryGridComponent* TargetComponent) {}
	virtual void ClearDragHighlight() {}

	virtual bool OnItemDropped(int32 ItemIndex, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) { return false; }
	virtual bool OnItemDroppedFromExternal(UItemDragDropOperation* DragOp, const FIntPoint& TargetPosition, bool bDropRotated, UInventoryGridComponent* TargetComponent) { return false; }

	// 드래그 중인 아이템 참조 저장
	virtual void SetActiveDragOperation(UItemDragDropOperation* InDragOp) { ActiveDragOp = InDragOp; }

	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 우클릭 컨텍스트 액션 (장착/사용)
	virtual void OnItemContextAction(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// Ctrl+우클릭 수량 지정 버리기
	virtual void OnItemRequestPartialDrop(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 장착 슬롯 UI 갱신
	virtual void RefreshEquipmentSlots() {}

	// 퀵슬롯 등록 UI 갱신
	virtual void RefreshQuickSlots() {}

	// 그리드 전체 재생성
	virtual void RefreshGrid(UInventoryGridComponent* Component) {}

	// 아이템 하나만 부분 갱신 (인덱스 밀림 감지 시 전체 재생성으로 폴백)
	virtual void RefreshSingleItem(UInventoryGridComponent* Component, int32 ItemIndex, const FGuid& ExpectedInstanceId) {}

protected:
	// 인덱스 유효성 확인 후 부분 갱신
	bool TryLightRefresh(UInventoryGridComponent* Comp, int32 ItemIndex, const FGuid& ExpectedInstanceId, const TArray<UInventoryItemWidget*>& ItemWidgets);

	// 그리드 패널 채우기 (슬롯+아이템 위젯 생성)
	void PopulateGridPanel(UGridPanel* GridPanel, UInventoryGridComponent* Comp,
		TSubclassOf<UInventorySlotWidget> SlotClass, TSubclassOf<UInventoryItemWidget> ItemClass,
		TMap<FIntPoint, UInventorySlotWidget*>& OutSlotMap, TArray<UInventoryItemWidget*>& OutItemWidgets);

	// 아이템 위젯 하나 갱신
	void RefreshItemWidgetInMap(int32 ItemIndex, UInventoryGridComponent* Comp, const TArray<UInventoryItemWidget*>& ItemWidgets);

	// 드래그 하이라이트 적용/해제
	static void ApplyDragHighlightToMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap, const FIntPoint& TargetTopLeft, const FIntPoint& Size, bool bIsValid);
	static void ClearHighlightMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap);

	// R키 회전
	bool HandleRotateDragKey();

	UPROPERTY()
	UItemDragDropOperation* ActiveDragOp = nullptr;
};
