#include "InventoryScreenWidget.h"
#include "Components/GridPanel.h"     // 일반 GridPanel용
#include "Components/GridSlot.h"      // Span 기능이 있는 일반 GridSlot용
#include "InventoryGridComponent.h"   // 가방 데이터 접근용

void UInventoryScreenWidget::InitializeGrid(UInventoryGridComponent* InInventoryComponent)
{
	if (!InInventoryComponent || !InventoryGridPanel || !SlotWidgetClass)
	{
		return;
	}

	InventoryGridPanel->ClearChildren();

	int32 Rows = InInventoryComponent->GridRows;
	int32 Columns = InInventoryComponent->GridColumns;

	for (int32 y = 0; y < Rows; ++y)
	{
		for (int32 x = 0; x < Columns; ++x)
		{
			// 3-A. 빈칸 슬롯 위젯 인스턴스 동적 생성
			UUserWidget* NewSlotWidget = CreateWidget<UUserWidget>(GetWorld(), SlotWidgetClass);
            
			if (NewSlotWidget)
			{
				// 3-B. 그리드 패널의 자식으로 밀어 넣고 GridSlot으로 받습니다.
				UGridSlot* GridSlot = InventoryGridPanel->AddChildToGrid(NewSlotWidget);
                
				if (GridSlot)
				{
					GridSlot->SetRow(y);
					GridSlot->SetColumn(x);
					
					GridSlot->SetPadding(FMargin(1.2f));
					
					GridSlot->SetHorizontalAlignment(HAlign_Fill);
					GridSlot->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
	}

	// 4. 아이템 위젯이 설정되어 있는지 확인
	if (!ItemWidgetClass)
	{
		return;
	}

	// 5. 가방 안의 아이템 렌더링 (대망의 Span 기능 적용)
	for (const FInventoryItemInstance& Item : InInventoryComponent->Items)
	{
		// 5-A. 아이템 위젯 생성
		UUserWidget* NewItemWidget = CreateWidget<UUserWidget>(GetWorld(), ItemWidgetClass);
		if (NewItemWidget)
		{
			// 5-B. 패널에 자식으로 추가
			UGridSlot* ItemGridSlot = InventoryGridPanel->AddChildToGrid(NewItemWidget);
			if (ItemGridSlot)
			{
				ItemGridSlot->SetColumn(Item.GridPosition.X);
				ItemGridSlot->SetRow(Item.GridPosition.Y);
                
				FIntPoint ItemSize = Item.GetEffectiveSize();
				ItemGridSlot->SetColumnSpan(ItemSize.X);
				ItemGridSlot->SetRowSpan(ItemSize.Y);
				
				ItemGridSlot->SetPadding(FMargin(1.0f));
				
				ItemGridSlot->SetLayer(1);
                
				ItemGridSlot->SetHorizontalAlignment(HAlign_Fill);
				ItemGridSlot->SetVerticalAlignment(VAlign_Fill);
			}
		}
	}
}