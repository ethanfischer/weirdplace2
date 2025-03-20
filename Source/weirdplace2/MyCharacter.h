// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MyCharacter.generated.h"

UCLASS()
class WEIRDPLACE2_API AMyCharacter : public ACharacter {
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	AMyCharacter();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void SetCanInteract(bool value);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Inventory")
	void         AddItemToInventory(EInventoryItem Item);
	virtual void AddItemToInventory_Implementation(EInventoryItem Item);

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh", meta = (AllowPrivateAccess = "true"))
	bool CanInteract = true;
};
