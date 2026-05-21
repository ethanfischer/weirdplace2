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

	// Back navigation. On a sub-page (Settings/Graphics) returns to Pause.
	// On the Pause page closes the menu. No-op when the menu isn't open.
	void HandleBack();

	// Vertical step nav (IA_PreviousOption / IA_NextOption) — d-pad up/down,
	// W/S, arrow up/down, left-stick up/down.
	void NavigatePrevious();
	void NavigateNext();

	// Horizontal step nav (IA_NavigateLeft / IA_NavigateRight) — d-pad left/right,
	// A/D, arrow left/right, left-stick left/right. Drives slider adjustment on
	// the Settings page; no-op on pages without horizontal selection.
	void AdjustLeft();
	void AdjustRight();

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

	void FreezePlayerMovement();
	void UnfreezePlayerMovement();
};
