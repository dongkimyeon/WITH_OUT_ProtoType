#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Interactable.generated.h"

class AProtoCharacter;

UINTERFACE(MinimalAPI, Blueprintable)
class UInteractable : public UInterface
{
	GENERATED_BODY()
};

class PROTOPROJECT_API IInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	void OnInteract(AProtoCharacter* InPlayer);

	UFUNCTION(BlueprintNativeEvent)
	FText GetInteractPrompt() const;

	UFUNCTION(BlueprintNativeEvent)
	bool CanInteract(AProtoCharacter* InPlayer) const;
};
