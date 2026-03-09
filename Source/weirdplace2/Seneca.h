#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "DlgSystem/DlgDialogueParticipant.h"
#include "GameFramework/Actor.h"
#include "Seneca.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UWidgetComponent;
class UDlgContext;
class UStaticMesh;
class UTexture2D;
class UChildActorComponent;
class UAnimSequenceBase;
class ADoor;
class AFirstPersonCharacter;

UENUM(BlueprintType)
enum class ESenecaState : uint8
{
	WaitingForMovies,       // "Buy 3 movies first"
	ReadyToGiveKey,         // "Nice picks, here's the key"
	GaveKey,                // "Go use the bathroom outside"
	Smoking,                // "Door's busted, use employee bathroom"
	AtEmployeeBathroom,     // "Here you go" + unlocks door
	Done                    // No more dialogue
};

UCLASS()
class WEIRDPLACE2_API ASeneca : public AActor, public IInteractable, public IDlgDialogueParticipant
{
	GENERATED_BODY()

public:
	ASeneca();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// IInteractable implementation
	virtual void Interact_Implementation() override;

	// IDlgDialogueParticipant implementation
	virtual FName GetParticipantName_Implementation() const override;
	virtual FText GetParticipantDisplayName_Implementation(FName ActiveSpeaker) const override;
	virtual ETextGender GetParticipantGender_Implementation() const override;
	virtual UTexture2D* GetParticipantIcon_Implementation(FName ActiveSpeaker, FName ActiveSpeakerState) const override;
	virtual bool CheckCondition_Implementation(const UDlgContext* Context, FName ConditionName) const override;
	virtual float GetFloatValue_Implementation(FName ValueName) const override;
	virtual int32 GetIntValue_Implementation(FName ValueName) const override;
	virtual bool GetBoolValue_Implementation(FName ValueName) const override;
	virtual FName GetNameValue_Implementation(FName ValueName) const override;
	virtual bool OnDialogueEvent_Implementation(UDlgContext* Context, FName EventName) override;
	virtual bool ModifyFloatValue_Implementation(FName ValueName, bool bDelta, float Value) override;
	virtual bool ModifyIntValue_Implementation(FName ValueName, bool bDelta, int32 Value) override;
	virtual bool ModifyBoolValue_Implementation(FName ValueName, bool bNewValue) override;
	virtual bool ModifyNameValue_Implementation(FName ValueName, FName NameValue) override;

	// Widget component hosting the dialogue UI - auto-found by name in BeginPlay
	UPROPERTY(BlueprintReadOnly, Category = "Seneca|Dialogue")
	UWidgetComponent* DialogueWidgetComponent;

	// --- Quest State ---

	// Called by OutsideBathroomDoor when the key is dropped
	void OnKeyDropped();

	// Called by FirstPersonCharacter when dialogue with Seneca ends
	void OnDialogueEnded();

	// Current quest state (read-only in editor for debugging)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Quest")
	ESenecaState CurrentState = ESenecaState::WaitingForMovies;

	// Set once the WaitingForMovies basket beat has fully played; prevents replaying on re-enter/re-interact
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "State")
	bool bIntroDialoguePlayed = false;

	// --- Animation ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Animation")
	UAnimSequenceBase* SmokingAnimation;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Animation")
	bool bIsSmoking = false;

protected:
	// Sphere overlap callbacks
	UFUNCTION()
	void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// --- Components (assigned in Blueprint) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca")
	USkeletalMeshComponent* BodyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca")
	USphereComponent* TriggerSphere;

	UPROPERTY(BlueprintReadOnly, Category = "Seneca")
	UChildActorComponent* CigaretteComp;

	// --- Dialogue per state (txt file paths relative to Content/) ---

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString WaitingForMoviesDialoguePath = TEXT("Dialogue/WaitingForMovies.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString ReadyToGiveKeyDialoguePath = TEXT("Dialogue/ReadyToGiveKey.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString GaveKeyDialoguePath = TEXT("Dialogue/GaveKey.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString SmokingDialoguePath = TEXT("Dialogue/Smoking.txt");

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString EmployeeBathroomDialoguePath = TEXT("Dialogue/EmployeeBathroom.txt");

	// Radius of the dialogue trigger sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	float DialogueTriggerRadius = 200.0f;

	// --- Key ---

	// Key name given to player
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FName KeyToGive = FName("Key");

	// Static mesh for the key (used for inventory visual data)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UStaticMesh* KeyMesh;

	// Scale override for the key in inventory/held view
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FVector KeyScale = FVector(0.001f, 0.001f, 0.001f);

	// Thumbnail shown in inventory after the key is dropped (broken key)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UTexture2D* KeyBrokenThumbnail;

	// --- Quest Config ---

	// Number of movies required before Seneca gives the key
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Quest")
	int32 RequiredMovieCount = 3;

	// Seconds after key drop before Seneca appears at smoking position
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Quest")
	float SmokingAppearDelay = 60.0f;

	// --- Position Targets (assign on level instance, these are level actor refs) ---

	// Empty actor placed at the smoking spot outside
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* SmokingPositionTarget;

	// Empty actor placed at the employee bathroom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* EmployeeBathroomPositionTarget;

	// --- Door Reference (assign on level instance) ---

	// The employee bathroom door (starts locked, unlocked by Seneca)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Door")
	ADoor* EmployeeBathroomDoor;

	// --- Basket Beat Config ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	UStaticMesh* ShoppingBasketMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	float BasketBeatDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	float BasketDistance = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	FVector BasketScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Basket")
	int32 BasketBeatLineIndex = 2;

	UPROPERTY(EditAnywhere, Category = "Seneca|Dialogue")
	FString InventoryButtonDisplayName = TEXT("Tab");

private:
	// Gives the key to the player
	void GiveKey();

	// Teleport Seneca to target actor's location/rotation
	void MoveToTarget(AActor* Target);

	// Returns the loaded dialogue lines for the current state
	const TArray<FText>* GetDialogueLinesForCurrentState() const;

	// Check if player has enough movies and update state accordingly
	void CheckMovieCount();

	// Loaded dialogue lines per state
	TMap<ESenecaState, TArray<FText>> DialogueLines;

	// Helper to load a single dialogue file
	void LoadDialogueFile(ESenecaState State, const FString& RelativePath);

	// Delegate listener for inventory changes
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Returns true if the player camera is facing the given world position
	bool IsPlayerLookingAt(const FVector& Position) const;

	// Returns true if the player camera is looking at Seneca
	bool IsPlayerLookingAtMe() const;

	// Deferred move: target to teleport to once player looks away
	AActor* PendingMoveTarget = nullptr;

	// Tracks that the player was looking at Seneca (requires look then look-away)
	bool bWasLookingAtMe = false;

	// Timer for delayed appearance at smoking position
	FTimerHandle SmokingAppearTimerHandle;

	// True while waiting for player to look away so Seneca can appear
	bool bWaitingToAppear = false;

	// Called when the smoking appear delay expires
	void OnSmokingDelayComplete();

	// --- Basket Beat ---

	void StartWaitingForMoviesDialogue(AFirstPersonCharacter* FPChar);

	UFUNCTION()
	void OnBasketDialogueLineShown(int32 LineIndex);

	void OnBasketBeatFinished();

	bool bBasketBeatArmed = false;
	FTimerHandle BasketBeatTimerHandle;

	UPROPERTY()
	AActor* SpawnedBasketActor = nullptr;
};
