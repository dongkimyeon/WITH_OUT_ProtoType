#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ContainerScreenWidget.generated.h"

class UInventoryGridComponent;
class UInventoryScreenWidget;

UCLASS(meta = (PrioritizeCategories = "Container UI"))
class PROTOPROJECT_API UContainerScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeScreen(UInventoryGridComponent* PlayerComp, UInventoryGridComponent* ContainerComp);

protected:
	// UMG에서 이름 일치 필요: PlayerInventoryWidget, ContainerInventoryWidget
	UPROPERTY(meta = (BindWidget))
	UInventoryScreenWidget* PlayerInventoryWidget;

	UPROPERTY(meta = (BindWidget))
	UInventoryScreenWidget* ContainerInventoryWidget;
};
