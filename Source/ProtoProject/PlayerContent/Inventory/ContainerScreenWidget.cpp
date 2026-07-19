#include "ContainerScreenWidget.h"
#include "InventoryScreenWidget.h"
#include "InventoryGridComponent.h"

void UContainerScreenWidget::InitializeScreen(UInventoryGridComponent* PlayerComp, UInventoryGridComponent* ContainerComp)
{
	if (PlayerInventoryWidget && PlayerComp)
	{
		PlayerInventoryWidget->InitializeGrid(PlayerComp);
	}
	if (ContainerInventoryWidget && ContainerComp)
	{
		ContainerInventoryWidget->InitializeGrid(ContainerComp);
	}
}
