#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SettingsUIComponent.generated.h"

class ASettingsUIActor;
class UWeirdplaceGameUserSettings;
class USoundBase;

UENUM(BlueprintType)
enum class ESettingsUIState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API USettingsUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USettingsUIComponent();

	UFUNCTION(BlueprintCallable, Category = "Settings UI")
	void ToggleSettingsUI();

	UFUNCTION(BlueprintCallable, Category = "Settings UI")
	void OpenSettingsUI();

	UFUNCTION(BlueprintCallable, Category = "Settings UI")
	void CloseSettingsUI();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings UI")
	bool IsSettingsOpen() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings UI")
	bool IsSettingsFullyOpen() const { return CurrentState == ESettingsUIState::Open; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Settings UI")
	bool IsSettingsFullyClosed() const { return CurrentState == ESettingsUIState::Closed; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Setup")
	TSubclassOf<ASettingsUIActor> SettingsUIActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Position")
	float SettingsDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Position")
	float VerticalOffset = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Animation")
	float AnimationDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Animation")
	float AnimationDropDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Audio")
	USoundBase* MenuOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings UI|Audio")
	USoundBase* MenuCloseSound;

private:
	UPROPERTY()
	ESettingsUIState CurrentState = ESettingsUIState::Closed;

	float AnimationProgress = 0.0f;

	FVector StoredUIPosition;
	FRotator StoredUIRotation;

	UPROPERTY()
	ASettingsUIActor* SettingsUIActor;

	UPROPERTY()
	UWeirdplaceGameUserSettings* CachedSettings;

	void SpawnSettingsUIActor();
	void DestroySettingsUIActor();
	void UpdateSettingsPosition();

	void BindNavigateInput();
	void UnbindNavigateInput();

	void HandleNavigateAxisX(float AxisValue);

	void FreezePlayerMovement();
	void UnfreezePlayerMovement();

	// Flick-step state for the X axis. True when ready to fire a step on next threshold cross.
	bool bArmedX = true;
};
