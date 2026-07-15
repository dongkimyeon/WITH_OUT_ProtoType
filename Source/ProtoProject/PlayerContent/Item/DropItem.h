// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemDataBase.h"
#include "DropItem.generated.h"

class UStaticMeshComponent;

UCLASS(meta = (PrioritizeCategories = "Data"))
class PROTOPROJECT_API ADropItem : public AActor
{
	GENERATED_BODY()
	
public:	
	ADropItem();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* StaticMeshComp;
	
	UPROPERTY(EditDefaultsOnly, Category = "Data")
	UItemDataBase* ItemData;
	
protected:
	virtual void OnConstruction(const FTransform& Transform) override;

	
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
