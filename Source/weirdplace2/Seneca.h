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

	// Called directly via DlgSystem "UnrealFunction" event type
	UFUNCTION(BlueprintCallable, Category = "Seneca|Key")
	void GiveKey();
};
