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
};
