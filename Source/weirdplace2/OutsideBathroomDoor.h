#pragma once

#include "CoreMinimal.h"
#include "Door.h"
#include "OutsideBathroomDoor.generated.h"

class ASeneca;
class USoundBase;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UTimelineComponent;
class UCurveFloat;

UCLASS()
class WEIRDPLACE2_API AOutsideBathroomDoor : public ADoor
{
	GENERATED_BODY()

public:
	AOutsideBathroomDoor();

	virtual void BeginPlay() override;

	// Override interact for bathroom-specific behavior
	virtual void Interact_Implementation() override;

protected:
	// Tracks whether the key has been dropped
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor")
	bool bDidDropKey = false;

	// Reference to Seneca so we can notify her when the key is dropped
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor")
	ASeneca* SenecaRef;

	// Sound to play when the key is dropped (legacy - kept for blueprint compat)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|Sounds")
	USoundBase* KeyDropSound;

	// Key name to remove from inventory (should match Seneca's KeyToGive)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor")
	FName KeyToRemove = FName("Key");

	// --- Key Break Animation ---

	// Scene component positioned at the keyhole - place in BP
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor|KeyAnim")
	USceneComponent* KeyLockSocket;

	// Timeline driving the key insert lerp (hand → lock)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor|KeyAnim")
	UTimelineComponent* KeyInsertTimeline;

	// Timeline driving the key turn (~90°)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor|KeyAnim")
	UTimelineComponent* KeyTurnTimeline;

	// How far from the keyhole the key appears before sliding in (cm)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	float KeyInsertStartOffset = 10.0f;

	// Local-space axis of KeyLockSocket the key approaches from (e.g. (1,0,0) = socket forward, (0,0,1) = socket up)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	FVector KeyInsertApproachAxis = FVector(1.0f, 0.0f, 0.0f);

	// How many cm short of the socket the key stops (pull back if inserting too deep)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	float KeyInsertEndOffset = 2.0f;

	// Rotation offset applied to the key mesh on top of the socket rotation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	FRotator KeyMeshRotationOffset = FRotator::ZeroRotator;

	// Local-space axis of KeyLockSocket to rotate around during the turn phase (e.g. (0,0,1) = socket up)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	FVector KeyTurnAxis = FVector(0.0f, 0.0f, 1.0f);

	// Degrees to rotate during the turn phase (negative = clockwise when viewed down the axis)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	float KeyTurnAngle = -90.0f;

	// Seconds after the turn completes before the broken key falls
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	float KeyFallDelay = 1.0f;

	// Easing curve for insert phase (ease-in, 0→1 over ~1.0s) - assign in BP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	UCurveFloat* KeyInsertCurve;

	// Easing curve for turn phase (ease-in-out, 0→1 over ~0.5s) - assign in BP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	UCurveFloat* KeyTurnCurve;

	// Temporary mesh used only during the animation sequence (hidden by default)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OutsideBathroomDoor|KeyAnim")
	UStaticMeshComponent* AnimKeyMesh;

	// Full key mesh shown during insert and turn phases - assign in BP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	UStaticMesh* FullKeyMesh;

	// Broken key half mesh shown after snap - assign in BP
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|KeyAnim")
	UStaticMesh* BrokenKeyMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|Sounds")
	USoundBase* KeyInsertSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|Sounds")
	USoundBase* KeyTurnSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OutsideBathroomDoor|Sounds")
	USoundBase* KeyBreakSound;

private:
	// World-space start position of the held item at sequence start
	FVector KeyAnimStartPos;

	// World-space start rotation of the held item at sequence start
	FRotator KeyAnimStartRot;

	// Materials captured from the key's inventory data (reused for BrokenKey entry)
	UPROPERTY()
	TArray<UMaterialInterface*> KeyMaterials;

	void StartKeyBreakSequence();

	UFUNCTION()
	void UpdateKeyInsert(float Alpha);

	UFUNCTION()
	void OnKeyInsertComplete();

	UFUNCTION()
	void UpdateKeyTurn(float Alpha);

	UFUNCTION()
	void OnKeyTurnComplete();

	UFUNCTION()
	void EnableKeyFall();

	FTimerHandle KeyFallTimerHandle;
};
