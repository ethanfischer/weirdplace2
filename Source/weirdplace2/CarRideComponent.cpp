#include "CarRideComponent.h"
#include "Rick.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "BladderUrgencyComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

UCarRideComponent::UCarRideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCarRideComponent::BeginPlay()
{
	Super::BeginPlay();
	StartRide();
}

void UCarRideComponent::StartRide()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: No PlayerController found"));
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: Player is not AFirstPersonCharacter"));
		return;
	}

	// Teleport player to passenger seat
	// Offset down by camera relative position so the player's eye (not feet) lands at the target
	if (PassengerSeatTarget)
	{
		FVector CameraOffset = FVector::ZeroVector;
		if (UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			CameraOffset = Camera->GetRelativeLocation();
		}
		Player->SetActorLocation(PassengerSeatTarget->GetActorLocation() - CameraOffset);
		Player->SetActorRotation(PassengerSeatTarget->GetActorRotation());
		PC->SetControlRotation(PassengerSeatTarget->GetActorRotation());
	}

	// Disable movement and interaction
	PC->SetIgnoreMoveInput(true);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetJumpAllowed(false);
	}
	Player->SetCanInteract(false);

	// Start scenery movement
	bSceneryMoving = true;
	SetComponentTickEnabled(true);

	// Schedule dialogue start
	GetWorld()->GetTimerManager().SetTimer(
		DialogueStartTimerHandle,
		this,
		&UCarRideComponent::StartDialogue,
		DialogueStartDelay,
		false
	);

	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Ride started, dialogue in %.1f seconds"), DialogueStartDelay);
}

void UCarRideComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bSceneryMoving && SceneryRoot)
	{
		FVector Delta = SceneryMoveDirection.GetSafeNormal() * ScenerySpeed * DeltaTime;
		SceneryRoot->AddActorWorldOffset(Delta);
	}
}

void UCarRideComponent::StartDialogue()
{
	if (!Rick)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: Rick reference is null"));
		return;
	}

	// Bind to Rick's dialogue ended delegate
	Rick->OnRickDialogueEnded.AddDynamic(this, &UCarRideComponent::OnDialogueEnded);

	// Enable interaction so player can advance dialogue with E
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (AMyCharacter* Player = Cast<AMyCharacter>(PC->GetPawn()))
		{
			Player->SetCanInteract(true);
		}
	}

	Rick->StartDialogue();
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Dialogue started"));
}

void UCarRideComponent::OnDialogueEnded()
{
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Dialogue ended, post-ride for %.1f seconds"), PostDialogueRideTime);

	// Disable interaction again during post-dialogue ride
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		if (AMyCharacter* Player = Cast<AMyCharacter>(PC->GetPawn()))
		{
			Player->SetCanInteract(false);
		}
	}

	// Schedule end of ride
	GetWorld()->GetTimerManager().SetTimer(
		PostDialogueTimerHandle,
		this,
		&UCarRideComponent::EndRide,
		PostDialogueRideTime,
		false
	);
}

void UCarRideComponent::EndRide()
{
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Fading to black"));

	// Fade camera to black
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(0.f, 1.f, FadeDuration, FLinearColor::Black, false, true);
	}

	// Schedule teleport after fade completes
	GetWorld()->GetTimerManager().SetTimer(
		FadeOutTimerHandle,
		this,
		&UCarRideComponent::OnFadeOutComplete,
		FadeDuration,
		false
	);
}

void UCarRideComponent::OnFadeOutComplete()
{
	// Stop scenery movement
	bSceneryMoving = false;
	SetComponentTickEnabled(false);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(PC->GetPawn());
	if (!Player)
	{
		return;
	}

	// Teleport player to arrival target (offset by camera height, same as seat teleport)
	if (ArrivalTarget)
	{
		FVector CameraOffset = FVector::ZeroVector;
		if (UCameraComponent* Camera = Player->GetFirstPersonCamera())
		{
			CameraOffset = Camera->GetRelativeLocation();
		}
		Player->SetActorLocation(ArrivalTarget->GetActorLocation() - CameraOffset);
		Player->SetActorRotation(ArrivalTarget->GetActorRotation());
		PC->SetControlRotation(ArrivalTarget->GetActorRotation());
	}

	// Re-enable movement
	PC->SetIgnoreMoveInput(false);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetJumpAllowed(true);
	}
	Player->SetCanInteract(true);

	// Start bladder urgency if it has delayed start
	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->StartUrgency();
	}

	// Fade camera back in
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, FadeDuration, FLinearColor::Black, false, false);
	}

	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Ride complete, player arrived"));
}
