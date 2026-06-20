#include "MenuUIComponent.h"
#include "MenuUIActor.h"
#include "WeirdplaceGameUserSettings.h"
#include "Components/SceneComponent.h"
#include "FirstPersonCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "MyCharacter.h"

// Panel dimensions match AMenuUIActor::UpdateBackgroundSize:
// width = 50 + padding*2, height = (kControllerHeaderZ+6) - (kSettingsBackZ-6) + padding*2.
static constexpr float kMenuLightWidth  = 58.0f;
static constexpr float kMenuLightHeight = 62.0f;

UMenuUIComponent::UMenuUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMenuUIComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!MenuUIActorClass)
	{
		MenuUIActorClass = AMenuUIActor::StaticClass();
	}

	CachedSettings = Cast<UWeirdplaceGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!CachedSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("UMenuUIComponent::BeginPlay - GameUserSettings is not UWeirdplaceGameUserSettings"));
	}
}

void UMenuUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(MenuActor))
	{
		MenuActor->Destroy();
		MenuActor = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void UMenuUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentState)
	{
	case EMenuUIState::Opening:
		AnimationProgress += DeltaTime / AnimationDuration;
		if (AnimationProgress >= 1.0f)
		{
			AnimationProgress = 1.0f;
			CurrentState = EMenuUIState::Open;
			// The menu UI is fully self-illuminated (all materials unlit/emissive),
			// so the player's inventory RectLight is no longer needed.
		}
		UpdateMenuPosition();
		break;

	case EMenuUIState::Closing:
		AnimationProgress -= DeltaTime / AnimationDuration;
		if (AnimationProgress <= 0.0f)
		{
			AnimationProgress = 0.0f;
			CurrentState = EMenuUIState::Closed;
			HideMenuActor();
			UnfreezePlayerMovement();

			if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
			{
				MyCharacter->SetCanInteract(true);
				MyCharacter->SetActivityState(EPlayerActivityState::FreeRoaming);
			}

			if (CachedSettings)
			{
				CachedSettings->SaveSettings();
				UE_LOG(LogTemp, Log, TEXT("Settings persisted: GamepadLookSensitivity=%.2f MouseLookSensitivity=%.2f"),
					CachedSettings->GetGamepadLookSensitivity(), CachedSettings->GetMouseLookSensitivity());
			}
		}
		else
		{
			UpdateMenuPosition();
		}
		break;

	case EMenuUIState::Open:
	case EMenuUIState::Closed:
		break;
	}
}

void UMenuUIComponent::ToggleMenu()
{
	if (CurrentState == EMenuUIState::Closed || CurrentState == EMenuUIState::Closing)
	{
		OpenMenu();
	}
	else
	{
		CloseMenu();
	}
}

void UMenuUIComponent::OpenMenu()
{
	if (CurrentState == EMenuUIState::Open || CurrentState == EMenuUIState::Opening)
	{
		return;
	}

	if (MenuOpenSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuOpenSound);
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("UMenuUIComponent::OpenMenu - no PlayerController"));
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector ForwardDir = CameraRotation.Vector();
	const FVector UpDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);

	StoredUIPosition = CameraLocation + ForwardDir * MenuDistance + UpDir * VerticalOffset;
	StoredUIRotation = CameraRotation;

	SpawnMenuActor();

	if (MenuActor)
	{
		MenuActor->SetPage(EMenuPage::Pause);
		if (CachedSettings)
		{
			MenuActor->SyncFromSettings(CachedSettings);
		}
	}

	if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
	{
		FirstPersonCharacter->SetInventoryFlashlightSize(kMenuLightWidth, kMenuLightHeight);
	}

	CurrentState = EMenuUIState::Opening;
	FreezePlayerMovement();

	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
		MyCharacter->SetActivityState(EPlayerActivityState::Interacting);
	}

	UE_LOG(LogTemp, Log, TEXT("Opening Menu UI"));
}

void UMenuUIComponent::CloseMenu()
{
	if (CurrentState == EMenuUIState::Closed || CurrentState == EMenuUIState::Closing)
	{
		return;
	}

	if (MenuCloseSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuCloseSound);
	}

	CurrentState = EMenuUIState::Closing;

	if (AFirstPersonCharacter* FirstPersonCharacter = Cast<AFirstPersonCharacter>(GetOwner()))
	{
		FirstPersonCharacter->SetInventoryFlashlightEnabled(false);
	}

	UE_LOG(LogTemp, Log, TEXT("Closing Menu UI"));
}

bool UMenuUIComponent::IsOpen() const
{
	return CurrentState == EMenuUIState::Open || CurrentState == EMenuUIState::Opening;
}

void UMenuUIComponent::HandleConfirm()
{
	if (CurrentState != EMenuUIState::Open)
	{
		return;
	}
	if (!MenuActor)
	{
		return;
	}

	switch (MenuActor->GetCurrentPage())
	{
	case EMenuPage::Pause:
		switch (MenuActor->GetSelectedPauseItem())
		{
		case EPauseMenuItem::Resume:
			CloseMenu();
			break;
		case EPauseMenuItem::Settings:
			MenuActor->SetPage(EMenuPage::Settings);
			if (CachedSettings)
			{
				MenuActor->SyncFromSettings(CachedSettings);
			}
			break;
		case EPauseMenuItem::Graphics:
			MenuActor->SetPage(EMenuPage::Graphics);
			MenuActor->SyncGraphicsFromCVars();
			break;
		case EPauseMenuItem::Quit:
			if (CachedSettings)
			{
				CachedSettings->SaveSettings();
				UE_LOG(LogTemp, Log, TEXT("Settings persisted on Quit: GamepadLookSensitivity=%.2f MouseLookSensitivity=%.2f"),
					CachedSettings->GetGamepadLookSensitivity(), CachedSettings->GetMouseLookSensitivity());
			}
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0))
			{
				UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
			}
			break;
		default:
			break;
		}
		break;

	case EMenuPage::Settings:
		if (MenuActor->GetSelectedSettingsRow() == ESettingsRow::Back)
		{
			MenuActor->SetPage(EMenuPage::Pause);
		}
		break;

	case EMenuPage::Graphics:
		switch (MenuActor->GetSelectedGraphicsRow())
		{
		case EGraphicsRow::ResetToDefault:
			MenuActor->ResetGraphicsToDefaults();
			break;
		case EGraphicsRow::Back:
			MenuActor->SetPage(EMenuPage::Pause);
			break;
		default:
			break;
		}
		break;
	}
}

void UMenuUIComponent::HandleBack()
{
	if (CurrentState != EMenuUIState::Open || !MenuActor)
	{
		return;
	}

	if (MenuActor->GetCurrentPage() == EMenuPage::Pause)
	{
		CloseMenu();
	}
	else
	{
		MenuActor->SetPage(EMenuPage::Pause);
	}
}

void UMenuUIComponent::NavigatePrevious()
{
	if (CurrentState != EMenuUIState::Open || !MenuActor)
	{
		return;
	}
	MenuActor->StepSelection(-1);
}

void UMenuUIComponent::NavigateNext()
{
	if (CurrentState != EMenuUIState::Open || !MenuActor)
	{
		return;
	}
	MenuActor->StepSelection(1);
}

void UMenuUIComponent::AdjustLeft()
{
	if (CurrentState != EMenuUIState::Open || !MenuActor || !CachedSettings)
	{
		return;
	}
	MenuActor->StepLeftRight(-1, CachedSettings);
}

void UMenuUIComponent::AdjustRight()
{
	if (CurrentState != EMenuUIState::Open || !MenuActor || !CachedSettings)
	{
		return;
	}
	MenuActor->StepLeftRight(1, CachedSettings);
}

void UMenuUIComponent::SpawnMenuActor()
{
	if (MenuActor)
	{
		if (USceneComponent* Root = MenuActor->GetRootComponent())
		{
			Root->SetVisibility(true, true);
		}
		MenuActor->SetActorEnableCollision(true);
		MenuActor->SetActorTickEnabled(true);
		// Re-apply page visibility because the SetVisibility(true, true) above
		// would otherwise show both pages.
		MenuActor->SetPage(EMenuPage::Pause);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !MenuUIActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("UMenuUIComponent::SpawnMenuActor - missing World or MenuUIActorClass"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	MenuActor = World->SpawnActor<AMenuUIActor>(MenuUIActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
}

void UMenuUIComponent::HideMenuActor()
{
	if (!MenuActor)
	{
		return;
	}
	if (USceneComponent* Root = MenuActor->GetRootComponent())
	{
		Root->SetVisibility(false, true);
	}
	MenuActor->SetActorEnableCollision(false);
	MenuActor->SetActorTickEnabled(false);
}

void UMenuUIComponent::UpdateMenuPosition()
{
	if (!MenuActor)
	{
		return;
	}

	const float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, AnimationProgress, 2.0f);
	const FVector UpDir = FRotationMatrix(StoredUIRotation).GetScaledAxis(EAxis::Z);
	const FVector AnimatedPosition = StoredUIPosition - UpDir * AnimationDropDistance * (1.0f - EasedProgress);

	MenuActor->SetActorLocation(AnimatedPosition);
	MenuActor->SetActorRotation(StoredUIRotation);
	MenuActor->SetOpacity(EasedProgress);
}

void UMenuUIComponent::FreezePlayerMovement()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->DisableMovement();
	}
}

void UMenuUIComponent::UnfreezePlayerMovement()
{
	ACharacter* Character = Cast<ACharacter>(GetOwner());
	if (!Character)
	{
		return;
	}
	if (UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		Move->SetMovementMode(MOVE_Walking);
	}
}
