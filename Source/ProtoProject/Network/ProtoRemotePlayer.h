#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProtoRemotePlayer.generated.h"

class UStaticMeshComponent;

// Fallback visual for a remote player, used only if BP_ProtoCharacter fails
// to load (see UProtoNetClientSubsystem::RemoteCharacterClass). Deliberately
// not an AProtoCharacter: that class's BeginPlay assumes a locally-controlled
// player (viewport UI, starter inventory, input binds), which would misfire here.
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
