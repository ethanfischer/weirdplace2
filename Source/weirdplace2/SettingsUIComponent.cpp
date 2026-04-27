#include "SettingsUIComponent.h"
#include "SettingsUIActor.h"
#include "WeirdplaceGameUserSettings.h"
#include "Components/SceneComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"
#include "MyCharacter.h"

USettingsUIComponent::USettingsUIComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USettingsUIComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!SettingsUIActorClass)
	{
		SettingsUIActorClass = ASettingsUIActor::StaticClass();
	}

	CachedSettings = Cast<UWeirdplaceGameUserSettings>(UGameUserSettings::GetGameUserSettings());
	if (!CachedSettings)
	{
		UE_LOG(LogTemp, Error, TEXT("USettingsUIComponent::BeginPlay - GameUserSettings is not UWeirdplaceGameUserSettings"));
	}
}

void USettingsUIComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SettingsUIActor))
	{
		SettingsUIActor->Destroy();
		SettingsUIActor = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void USettingsUIComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	switch (CurrentState)
	{
	case ESettingsUIState::Opening:
		AnimationProgress += DeltaTime / AnimationDuration;
		if (AnimationProgress >= 1.0f)
		{
			AnimationProgress = 1.0f;
			CurrentState = ESettingsUIState::Open;
		}
		UpdateSettingsPosition();
		break;

	case ESettingsUIState::Closing:
		AnimationProgress -= DeltaTime / AnimationDuration;
		if (AnimationProgress <= 0.0f)
		{
			AnimationProgress = 0.0f;
			CurrentState = ESettingsUIState::Closed;
			DestroySettingsUIActor();
			UnbindNavigateInput();
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
			UpdateSettingsPosition();
		}
		break;

	case ESettingsUIState::Open:
	case ESettingsUIState::Closed:
		break;
	}
}

void USettingsUIComponent::ToggleSettingsUI()
{
	if (CurrentState == ESettingsUIState::Closed || CurrentState == ESettingsUIState::Closing)
	{
		OpenSettingsUI();
	}
	else
	{
		CloseSettingsUI();
	}
}

void USettingsUIComponent::OpenSettingsUI()
{
	if (CurrentState == ESettingsUIState::Open || CurrentState == ESettingsUIState::Opening)
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
		UE_LOG(LogTemp, Error, TEXT("USettingsUIComponent::OpenSettingsUI - no PlayerController"));
		return;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector ForwardDir = CameraRotation.Vector();
	const FVector UpDir = FRotationMatrix(CameraRotation).GetScaledAxis(EAxis::Z);

	StoredUIPosition = CameraLocation + ForwardDir * SettingsDistance + UpDir * VerticalOffset;
	StoredUIRotation = CameraRotation;

	SpawnSettingsUIActor();

	if (SettingsUIActor && CachedSettings)
	{
		SettingsUIActor->SyncFromSettings(CachedSettings);
	}

	CurrentState = ESettingsUIState::Opening;
	bArmedX = true;
	bArmedY = true;
	FreezePlayerMovement();
	BindNavigateInput();

	if (AMyCharacter* MyCharacter = Cast<AMyCharacter>(GetOwner()))
	{
		MyCharacter->SetCanInteract(false);
		MyCharacter->SetActivityState(EPlayerActivityState::Interacting);
	}

	UE_LOG(LogTemp, Log, TEXT("Opening Settings UI"));
}

void USettingsUIComponent::CloseSettingsUI()
{
	if (CurrentState == ESettingsUIState::Closed || CurrentState == ESettingsUIState::Closing)
	{
		return;
	}

	if (MenuCloseSound)
	{
		UGameplayStatics::PlaySound2D(this, MenuCloseSound);
	}

	CurrentState = ESettingsUIState::Closing;
	UE_LOG(LogTemp, Log, TEXT("Closing Settings UI"));
}

bool USettingsUIComponent::IsSettingsOpen() const
{
	return CurrentState == ESettingsUIState::Open || CurrentState == ESettingsUIState::Opening;
}

void USettingsUIComponent::SpawnSettingsUIActor()
{
	if (SettingsUIActor)
	{
		if (USceneComponent* Root = SettingsUIActor->GetRootComponent())
		{
			Root->SetVisibility(true, true);
		}
		SettingsUIActor->SetActorEnableCollision(true);
		SettingsUIActor->SetActorTickEnabled(true);
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !SettingsUIActorClass)
	{
		UE_LOG(LogTemp, Error, TEXT("USettingsUIComponent::SpawnSettingsUIActor - missing World or SettingsUIActorClass"));
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.Owner = GetOwner();

	SettingsUIActor = World->SpawnActor<ASettingsUIActor>(SettingsUIActorClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
}

void USettingsUIComponent::DestroySettingsUIActor()
{
	if (!SettingsUIActor)
	{
		return;
	}
	if (USceneComponent* Root = SettingsUIActor->GetRootComponent())
	{
		Root->SetVisibility(false, true);
	}
	SettingsUIActor->SetActorEnableCollision(false);
	SettingsUIActor->SetActorTickEnabled(false);
}

void USettingsUIComponent::UpdateSettingsPosition()
{
	if (!SettingsUIActor)
	{
		return;
	}

	const float EasedProgress = FMath::InterpEaseInOut(0.0f, 1.0f, AnimationProgress, 2.0f);
	const FVector UpDir = FRotationMatrix(StoredUIRotation).GetScaledAxis(EAxis::Z);
	const FVector AnimatedPosition = StoredUIPosition - UpDir * AnimationDropDistance * (1.0f - EasedProgress);

	SettingsUIActor->SetActorLocation(AnimatedPosition);
	SettingsUIActor->SetActorRotation(StoredUIRotation);
	SettingsUIActor->SetOpacity(EasedProgress);
}

void USettingsUIComponent::BindNavigateInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("USettingsUIComponent::BindNavigateInput - no PC/InputComponent"));
		return;
	}

	PC->InputComponent->BindAxis("Move Right / Left", this, &USettingsUIComponent::HandleNavigateAxisX);
	PC->InputComponent->BindAxis("Move Forward / Backward", this, &USettingsUIComponent::HandleNavigateAxisY);
}

void USettingsUIComponent::UnbindNavigateInput()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC || !PC->InputComponent)
	{
		return;
	}
	// Mirrors AMovieBox::StopInspection - clears legacy axis bindings wholesale.
	PC->InputComponent->AxisBindings.Empty();
}

void USettingsUIComponent::HandleNavigateAxisX(float AxisValue)
{
	if (CurrentState != ESettingsUIState::Open && CurrentState != ESettingsUIState::Opening)
	{
		return;
	}
	if (!SettingsUIActor || !CachedSettings)
	{
		return;
	}

	constexpr float FireThreshold = 0.5f;
	constexpr float RearmThreshold = 0.2f;

	const float AbsValue = FMath::Abs(AxisValue);
	if (!bArmedX)
	{
		if (AbsValue < RearmThreshold)
		{
			bArmedX = true;
		}
		return;
	}

	if (AbsValue > FireThreshold)
	{
		bArmedX = false;
		const int32 Delta = AxisValue > 0.0f ? 1 : -1;
		SettingsUIActor->StepSelection(Delta, CachedSettings);
	}
}

void USettingsUIComponent::HandleNavigateAxisY(float AxisValue)
{
	if (CurrentState != ESettingsUIState::Open && CurrentState != ESettingsUIState::Opening)
	{
		return;
	}
	if (!SettingsUIActor)
	{
		return;
	}

	constexpr float FireThreshold = 0.5f;
	constexpr float RearmThreshold = 0.2f;

	const float AbsValue = FMath::Abs(AxisValue);
	if (!bArmedY)
	{
		if (AbsValue < RearmThreshold)
		{
			bArmedY = true;
		}
		return;
	}

	if (AbsValue > FireThreshold)
	{
		bArmedY = false;
		// Positive Y = forward/up = move to previous row (Delta -1).
		const int32 Delta = AxisValue > 0.0f ? -1 : 1;
		SettingsUIActor->StepFocusedRow(Delta);
	}
}

void USettingsUIComponent::FreezePlayerMovement()
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

void USettingsUIComponent::UnfreezePlayerMovement()
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
