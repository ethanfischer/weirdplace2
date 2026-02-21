#pragma once

#include "CoreMinimal.h"
#include "Interactable.h"
#include "DlgSystem/DlgDialogueParticipant.h"
#include "GameFramework/Actor.h"
#include "Seneca.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UWidgetComponent;
class UDlgDialogue;
class UDlgContext;
class UStaticMesh;
class ADoor;

UCLASS()
class WEIRDPLACE2_API ASeneca : public AActor, public IInteractable, public IDlgDialogueParticipant
{
	GENERATED_BODY()

public:
	ASeneca();

protected:
	virtual void BeginPlay() override;

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

	// --- Properties ---

	// Dialogue asset to start when player enters sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	UDlgDialogue* Dialogue;

	// Radius of the dialogue trigger sphere
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	float DialogueTriggerRadius = 200.0f;

	// Key name given to player via "GiveKey" dialogue event
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FName KeyToGive = FName("Key");

	// Static mesh for the key (used for inventory visual data)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	UStaticMesh* KeyMesh;

	// Scale override for the key in inventory/held view
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Key")
	FVector KeyScale = FVector(0.001f, 0.001f, 0.001f);

	// Called directly via DlgSystem "UnrealFunction" event type
	UFUNCTION(BlueprintCallable, Category = "Seneca|Key")
	void GiveKey();

public:
	// --- Quest State ---

	// Called by OutsideBathroomDoor when the key is dropped
	void OnKeyDropped();

	// Teleport Seneca to the smoking position and switch dialogue
	void MoveToSmokingPosition();

	// Teleport Seneca to the employee bathroom position and switch dialogue
	void MoveToEmployeeBathroomPosition();

	// Unlock the employee bathroom door
	void UnlockEmployeeBathroomDoor();

protected:
	// --- Quest State Storage ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Quest")
	TMap<FName, bool> BoolValues;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Seneca|Quest")
	TMap<FName, int32> IntValues;

	// Number of movies required before Seneca gives the key
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Quest")
	int32 RequiredMovieCount = 3;

	// --- Position Targets ---

	// Empty actor placed at the smoking spot outside
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* SmokingPositionTarget;

	// Empty actor placed at the employee bathroom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Positions")
	AActor* EmployeeBathroomPositionTarget;

	// --- Door Reference ---

	// The employee bathroom door (starts locked, unlocked via dialogue)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Door")
	ADoor* EmployeeBathroomDoor;

	// --- Additional Dialogues ---

	// Dialogue used when Seneca is at the smoking spot
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	UDlgDialogue* SmokingDialogue;

	// Dialogue used when Seneca is at the employee bathroom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seneca|Dialogue")
	UDlgDialogue* EmployeeBathroomDialogue;

private:
	// Delegate listener for inventory changes
	UFUNCTION()
	void OnInventoryChanged(const TArray<FName>& CurrentItems);

	// Teleport Seneca to target actor's location/rotation
	void MoveToTarget(AActor* Target);

	// Update MovieCount from player inventory
	void UpdateMovieCount();
};
