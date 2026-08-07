#include "ProtoRemotePlayer.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AProtoRemotePlayer::AProtoRemotePlayer()
{
	PrimaryActorTick.bCanEverTick = false;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetGenerateOverlapEvents(false);

	// A basic engine shape so this works out of the box with no project
	// content dependency. Only used if BP_ProtoCharacter fails to load (see
	// UProtoNetClientSubsystem::RemoteCharacterClass); the normal case spawns
	// the real character mesh instead of this placeholder.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMeshFinder.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMeshFinder.Object);
		BodyMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.8f));
		BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	}
}
