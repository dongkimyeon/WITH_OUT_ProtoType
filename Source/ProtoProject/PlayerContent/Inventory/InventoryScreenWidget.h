#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InventoryScreenWidget.generated.h"

// 전방 선언을 통해 컴파일 속도를 최적화합니다.
class UUniformGridPanel;
class UInventoryGridComponent;
class UGridPanel;
UCLASS()
class PROTOPROJECT_API UInventoryScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 외부(캐릭터 등)에서 인벤토리 컴포넌트를 넘겨받아 격자를 생성하는 함수
	UFUNCTION(BlueprintCallable, Category = "Inventory UI")
	void InitializeGrid(UInventoryGridComponent* InInventoryComponent);

protected:
	// 2. 바인딩 변수 타입 변경
	UPROPERTY(meta = (BindWidget))
	UGridPanel* InventoryGridPanel; // (기존 UUniformGridPanel 에서 변경)
	
	// 격자를 채울 '한 칸짜리 슬롯'의 위젯 클래스 에셋 (에디터 블루프린트에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UUserWidget> SlotWidgetClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Inventory UI")
	TSubclassOf<UUserWidget> ItemWidgetClass;
	
};