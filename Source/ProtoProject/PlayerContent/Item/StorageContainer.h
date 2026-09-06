#pragma once

#include "CoreMinimal.h"
#include "ItemContainerBase.h"
#include "../../Network/ProtoNetClientSubsystem.h"
#include "StorageContainer.generated.h"

// 플레이어가 아이템을 직접 넣고 빼는 개인 보관함(안전지대 창고). 계정별로 서버 DB
// (dbo.PlayerStash)에 저장되어 재접속·레벨 이동 후에도 유지된다 -- BeginPlay에서
// 서버에 저장된 내용을 요청해 채우고(HandleStashState), 이후 내용이 바뀔 때마다
// (넣기/빼기/이동 -- ContainerInventory->OnInventoryChanged) 즉시 전체 스냅샷을
// 서버에 저장한다(HandleStashChanged). 다른 루팅 상자(ALootContainer)와 달리 계정
// 전용 데이터라서 NetSlotId를 부여하지 않는다 -- 동시에 같은 계정 창고를 만질 수
// 있는 다른 플레이어가 없으니 first-claim-wins 조정이 필요 없고, 넣고 빼는 것도
// 그냥 로컬에서 즉시 처리된다(InventoryScreenWidget의 크로스그리드 이동 로직이
// NetSlotId == 0인 아이템을 다루는 방식 그대로).
UCLASS()
class PROTOPROJECT_API AStorageContainer : public AItemContainerBase
{
	GENERATED_BODY()

public:
	AStorageContainer();

protected:
	virtual void BeginPlay() override;

	// SeedContents는 오버라이드하지 않는다 -- 베이스의 "빈 채로 시작"이 정확히 맞다:
	// 서버 응답이 올 때까지는 빈 게 맞고, 응답이 오면 HandleStashState가 채운다.

private:
	// C2S_RequestStash의 답(S2C_StashState)으로 한 번만 온다 -- 계정당 유니캐스트라
	// ALootContainer::HandleContainerLootState처럼 컨테이너 id로 걸러낼 필요가 없다.
	UFUNCTION()
	void HandleStashState(const TArray<FProtoInventoryItemEntry>& Items);

	// ContainerInventory->OnInventoryChanged에 바인딩 -- 넣기/빼기/이동 등 그리드가
	// 바뀔 때마다 전체 스냅샷을 즉시 서버에 저장한다(SendSaveInventory와 동일한
	// "항상 전체, diff 아님" 방식).
	UFUNCTION()
	void HandleStashChanged();

	// HandleStashState가 ContainerInventory를 채우는 동안(Items.Empty() + 여러 번의
	// AddItemAt) HandleStashChanged가 그 중간 상태들을 서버에 재저장하지 않도록
	// 억제한다 -- AProtoCharacter::bIsRestoringInventory와 같은 목적.
	bool bIsRestoringStash = false;
};
