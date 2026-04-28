#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MenuUIComponent.generated.h"

class AMenuUIActor;
class UWeirdplaceGameUserSettings;
class USoundBase;

UENUM(BlueprintType)
enum class EMenuUIState : uint8
{
	Closed,
	Opening,
	Open,
	Closing
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class WEIRDPLACE2_API UMenuUIComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMenuUIComponent();

	UFUNCTION(BlueprintCallable, Category = "Menu UI")
	void ToggleMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu UI")
	void OpenMenu();

	UFUNCTION(BlueprintCallable, Category = "Menu UI")
	void CloseMenu();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Menu UI")
	bool IsOpen() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Menu UI")
	bool IsFullyOpen() const { return CurrentState == EMenuUIState::Open; }

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Menu UI")
	bool IsFullyClosed() const { return CurrentState == EMenuUIState::Closed; }

	// Test-only accessor.
	AMenuUIActor* GetMenuActor() const { return MenuActor; }

	// Called by the owning character when the Interact action triggers while the
	// menu is open. Acts on the currently selected item on the active page.
	void HandleConfirm();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Setup")
	TSubclassOf<AMenuUIActor> MenuUIActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Position")
	float MenuDistance = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Position")
	float VerticalOffset = -15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Animation")
	float AnimationDuration = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Animation")
	float AnimationDropDistance = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Audio")
	USoundBase* MenuOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Menu UI|Audio")
	USoundBase* MenuCloseSound;

private:
	UPROPERTY()
	EMenuUIState CurrentState = EMenuUIState::Closed;

	float AnimationProgress = 0.0f;

	FVector StoredUIPosition;
	FRotator StoredUIRotation;

	UPROPERTY()
	AMenuUIActor* MenuActor;

	UPROPERTY()
	UWeirdplaceGameUserSettings* CachedSettings;

	void SpawnMenuActor();
	void HideMenuActor();
	void UpdateMenuPosition();

	void BindMenuInput();
	void UnbindMenuInput();

	void HandleNavigateAxisX(float AxisValue);
	void HandleNavigateAxisY(float AxisValue);

	void FreezePlayerMovement();
	void UnfreezePlayerMovement();

	bool bArmedX = true;
	bool bArmedY = true;
};
