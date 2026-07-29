#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProtoRemotePlayer.generated.h"

class UStaticMeshComponent;

// Visual placeholder for another connected player, positioned from
// S2C_SendPlayerInfo / S2C_MoveState by UProtoNetClientSubsystem.
//
// Deliberately NOT an AProtoCharacter: that class's BeginPlay assumes a
// locally-controlled player (adds UI to the viewport, grants starter
// inventory, binds input, ...), all of which would misfire for a remote
// player spawned with no controller. This is just a simple mesh stand-in.
UCLASS()
class PROTOPROJECT_API AProtoRemotePlayer : public AActor
{
	GENERATED_BODY()

public:
	AProtoRemotePlayer();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "ProtoNet")
	UStaticMeshComponent* BodyMesh;

	// Not a UPROPERTY: plain data, not a UObject reference, no reflection needed.
	uint32 PlayerId = 0;
};
