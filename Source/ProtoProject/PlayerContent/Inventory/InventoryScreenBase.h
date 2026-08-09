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

	// 드래그 중인 아이템 참조 - R키 회전 등 공용 로직(HandleRotateDragKey)에서 쓴다.
	// 기본 구현이 그대로 필요한 로직이라 서브클래스는 오버라이드할 필요 없다.
	virtual void SetActiveDragOperation(UItemDragDropOperation* InDragOp) { ActiveDragOp = InDragOp; }

	virtual void OnItemHoverBegin(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}
	virtual void OnItemHoverEnd(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 아이템 우클릭 시 호출됨 - 아이템 데이터의 GetContextAction()에 따라 장착/사용 등으로 분기
	virtual void OnItemContextAction(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 아이템 Ctrl+우클릭 시 호출됨 - 스택 아이템이면 수량을 지정해서 버리는 팝업을 띄운다.
	virtual void OnItemRequestPartialDrop(int32 ItemIndex, UInventoryGridComponent* OwningComponent) {}

	// 장착 슬롯 UI를 다시 그린다 (장착/해제/스왑 이후 호출)
	virtual void RefreshEquipmentSlots() {}

	// 퀵슬롯 등록 UI를 다시 그린다 (등록/해제/스왑 이후 호출)
	virtual void RefreshQuickSlots() {}

	virtual void RefreshGrid(UInventoryGridComponent* Component) {}

	// 아이템 하나의 위치/회전/수량만 바뀌었을 때 그리드 전체를 다시 만들지 않고 그 위젯만 갱신한다.
	// ExpectedInstanceId가 그 인덱스의 실제 아이템과 다르면(같은 그리드 안에서의 병합 등으로 다른 아이템이
	// 배열에서 제거되어 인덱스가 밀린 경우) 안전하게 RefreshGrid로 전체를 다시 만든다.
	virtual void RefreshSingleItem(UInventoryGridComponent* Component, int32 ItemIndex, const FGuid& ExpectedInstanceId) {}

protected:
	// ItemIndex 위치의 아이템이 여전히 ExpectedInstanceId와 같으면(=인덱스가 밀리지 않았으면) 가벼운 갱신만 하고 true를 반환한다.
	// 이미 다른 아이템으로 밀렸거나 범위를 벗어났으면(=배열에서 제거됨) 아무 것도 하지 않고 false를 반환하니,
	// 호출부는 false일 때 RefreshGrid로 전체를 다시 만들어야 한다.
	bool TryLightRefresh(UInventoryGridComponent* Comp, int32 ItemIndex, const FGuid& ExpectedInstanceId, const TArray<UInventoryItemWidget*>& ItemWidgets);

	// 그리드 패널 하나(슬롯 위젯 + 아이템 위젯)를 처음부터 다시 채운다.
	// 그리드 하나짜리 화면(InventoryScreenWidget)과 여러 개짜리 화면(ContainerScreenWidget)이 공용으로 쓴다.
	void PopulateGridPanel(UGridPanel* GridPanel, UInventoryGridComponent* Comp,
		TSubclassOf<UInventorySlotWidget> SlotClass, TSubclassOf<UInventoryItemWidget> ItemClass,
		TMap<FIntPoint, UInventorySlotWidget*>& OutSlotMap, TArray<UInventoryItemWidget*>& OutItemWidgets);

	// 아이템 위젯 하나의 그리드 위치/크기만 갱신한다 (그리드 전체 재생성 없이).
	void RefreshItemWidgetInMap(int32 ItemIndex, UInventoryGridComponent* Comp, const TArray<UInventoryItemWidget*>& ItemWidgets);

	// 지정한 슬롯 맵에 드래그 하이라이트를 적용/해제한다.
	static void ApplyDragHighlightToMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap, const FIntPoint& TargetTopLeft, const FIntPoint& Size, bool bIsValid);
	static void ClearHighlightMap(TMap<FIntPoint, UInventorySlotWidget*>& SlotMap);

	// R키로 드래그 중인 아이템을 회전시킨다. 드래그 중이 아니면 아무 것도 하지 않고 false 반환.
	bool HandleRotateDragKey();

	UPROPERTY()
	UItemDragDropOperation* ActiveDragOp = nullptr;
};
