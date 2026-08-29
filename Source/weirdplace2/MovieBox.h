// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "FirstPersonCharacter.h"
class UTextRenderComponent;
class UDiegeticTextComponent;
#include "Components/WidgetComponent.h"
#include "GameFramework/Actor.h"
#include "MovieBox.generated.h"

UCLASS()
class WEIRDPLACE2_API AMovieBox : public AActor, public IInteractable {
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AMovieBox();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// IInteractable implementation
	virtual void Interact_Implementation() override;
	virtual bool CanInteract() override;

	void CollectInspectedMovie();
	void RotateInspectedActor(float AxisValue);
	void StopInspection();

	// Test-only query: true while this MovieBox is the actively inspected one.
	bool IsBeingInspected() const { return InspectedActor != nullptr; }

	bool WasCollected() const { return DidCollectMovie; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movie")
	UMaterialInterface* CoverMaterial;

	// Subclasses (e.g. BP_BlankVHS) set this to true to opt out of the 3-movie cap
	// and the IsMovieCollectionLocked() gate so they can be picked up after the cap is locked.
	UPROPERTY(EditDefaultsOnly, Category="Movie")
	bool bExemptFromMovieLimit = false;

	// If non-empty, used as the inventory ItemID instead of the actor-name-derived one.
	UPROPERTY(EditDefaultsOnly, Category="Movie")
	FName ItemIDOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction")
	float InspectionDistance = 50.0f;

private:
	AActor*    InspectedActor;
	FTransform OriginalActorTransform;
	FRotator   CameraRotation;
	APlayerController* PlayerController;
	AFirstPersonCharacter* MyCharacter;

	// Lazily resolves (and caches) the player character. MovieBoxes can
	// BeginPlay before the pawn spawns, so caching there is a race.
	AFirstPersonCharacter* GetMyCharacter();
	bool DidCollectMovie = false;
	FTimerHandle CantCarryTimerHandle;

	// Keeps the async cover-material load request alive until it completes.
	TSharedPtr<struct FStreamableHandle> CoverLoadHandle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI", meta=(AllowPrivateAccess="true"))
	UWidgetComponent* InteractionWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI", meta=(AllowPrivateAccess="true"))
	UTextRenderComponent* CantCarryWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Mesh", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* EnvelopeMesh;

	// World-space prompt naming the put-back binding, shown during inspection.
	// Added as a "PutBackPrompt" component in BP_MovieBox (so its position is
	// tunable in-editor); fetched by name in BeginPlay. Faces the player via
	// the component itself.
	UPROPERTY()
	UDiegeticTextComponent* PutBackPrompt = nullptr;

	// "[Q]  put back" or "[B]  put back" — the active device's 'Exit Interaction'
	// binding only.
	FString BuildPutBackPromptText() const;

	// Device the prompt text was last built for, so Tick can rebuild it when
	// the player switches devices mid-inspection.
	bool bPromptBuiltForGamepad = false;
};
