#include "CarRideComponent.h"
#include "Rick.h"
#include "UI_Dialogue.h"
#include "Components/WidgetComponent.h"
#include "FirstPersonCharacter.h"
#include "MyCharacter.h"
#include "BladderUrgencyComponent.h"
#include "MovieBox.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#endif

UCarRideComponent::UCarRideComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UCarRideComponent::BeginPlay()
{
	Super::BeginPlay();

	bool bSkipRide = false;
#if WITH_EDITOR
	if (const ULevelEditorPlaySettings* PlaySettings = GetDefault<ULevelEditorPlaySettings>())
	{
		bSkipRide = PlaySettings->LastExecutedPlayModeLocation == PlayLocation_CurrentCameraLocation;
	}
#endif

	// Poll until the player pawn is the right type, then start (or skip) the ride
	GetWorld()->GetTimerManager().SetTimer(
		DialogueStartTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this, bSkipRide]()
		{
			APlayerController* PC = GetWorld()->GetFirstPlayerController();
			if (PC && Cast<AFirstPersonCharacter>(PC->GetPawn()))
			{
				if (bSkipRide)
				{
					SkipRide();
				}
				else
				{
					StartRide();
				}
			}
		}),
		0.1f,
		true
	);
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

	// Disable collision on scenery so it doesn't block player spawn or capsule
	if (SceneryRoot)
	{
		SceneryRoot->SetActorEnableCollision(false);
	}

	// Hide the gas station during the ride (must iterate children; SetActorHiddenInGame doesn't propagate)
	if (GasStationRoot)
	{
		TArray<AActor*> GasStationActors;
		GasStationRoot->GetAttachedActors(GasStationActors, /*bResetArray=*/true, /*bRecursively=*/true);
		for (AActor* Actor : GasStationActors)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
	}

	// Hide all spawned movie boxes during the ride
	{
		TArray<AActor*> MovieBoxes;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMovieBox::StaticClass(), MovieBoxes);
		for (AActor* Actor : MovieBoxes)
		{
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
		}
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
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent: PassengerSeatTarget is null!"));
	}

	// Disable movement, gravity, and interaction
	PC->SetIgnoreMoveInput(true);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetJumpAllowed(false);
		MoveComp->GravityScale = 0.0f;
		MoveComp->Velocity = FVector::ZeroVector;
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

}

void UCarRideComponent::SkipRide()
{
	GetWorld()->GetTimerManager().ClearTimer(DialogueStartTimerHandle);
	UE_LOG(LogTemp, Log, TEXT("CarRideComponent: Skipping car ride (Spawn At Camera Location)"));

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	AFirstPersonCharacter* Player = PC ? Cast<AFirstPersonCharacter>(PC->GetPawn()) : nullptr;
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipRide - No player"));
		return;
	}

	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->StartUrgency();
	}

	if (Rick)
	{
		Rick->AppearOutside();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("CarRideComponent::SkipRide - Rick is null"));
	}
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

	// Rick may be a child of GasStationRoot and was hidden with it — make him visible now
	Rick->SetActorHiddenInGame(false);

	// Move dialogue widget to windshield target so player can see it from passenger seat.
	// Cache the original relative transform so we can restore it after the ride ends.
	if (DialogueWidgetTarget && Rick->DialogueWidgetComponent)
	{
		AActor* WidgetActor = Rick->DialogueWidgetComponent->GetOwner();
		if (WidgetActor && WidgetActor->GetRootComponent())
		{
			CachedWidgetRelativeLocation = WidgetActor->GetRootComponent()->GetRelativeLocation();
			CachedWidgetRelativeRotation = WidgetActor->GetRootComponent()->GetRelativeRotation();
			WidgetActor->SetActorLocationAndRotation(
				DialogueWidgetTarget->GetActorLocation(),
				DialogueWidgetTarget->GetActorRotation()
			);
		}
	}

	// Set dark text color for car ride dialogue readability
	if (Rick->DialogueWidgetComponent)
	{
		if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
		{
			DialogueWidget->SetTextColor(DialogueTextColor);
		}
	}

	// Bind to player's dialogue line delegate for bladder pulse trigger
	if (PC)
	{
		if (AFirstPersonCharacter* FPPlayer = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			FPPlayer->OnDialogueLineShown.AddDynamic(this, &UCarRideComponent::OnDialogueLineShown);
		}
	}

	Rick->StartDialogue();
}

void UCarRideComponent::OnDialogueEnded()
{
	// Unbind so future Rick dialogues (money, idle) don't re-trigger the car-ride end sequence
	Rick->OnRickDialogueEnded.RemoveDynamic(this, &UCarRideComponent::OnDialogueEnded);

	// Disable interaction again during post-dialogue ride
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (PC)
	{
		// Unbind bladder pulse listener — car ride dialogue is over
		if (AFirstPersonCharacter* FPPlayer = Cast<AFirstPersonCharacter>(PC->GetPawn()))
		{
			FPPlayer->OnDialogueLineShown.RemoveDynamic(this, &UCarRideComponent::OnDialogueLineShown);
		}

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

void UCarRideComponent::OnDialogueLineShown(int32 LineIndex)
{
	if (!Rick || Rick->BladderPulseLineIndex == INDEX_NONE || LineIndex != Rick->BladderPulseLineIndex)
	{
		return;
	}

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

	if (!bBladderPulseArmed)
	{
		// First broadcast: line was just displayed normally.
		// Arm the block so the next E press triggers the pulse beat instead of advancing.
		bBladderPulseArmed = true;
		Player->bBlockNextDialogueAdvance = true;
		return;
	}

	// Second broadcast: player pressed E, advance was blocked, dialogue closed.
	// Fire the pulse as its own beat, then auto-advance after it finishes.
	bBladderPulseArmed = false;
	Player->SetCanInteract(false);

	if (UBladderUrgencyComponent* BladderComp = Player->FindComponentByClass<UBladderUrgencyComponent>())
	{
		BladderComp->FireSinglePulse();

		GetWorld()->GetTimerManager().SetTimer(
			BladderPulseTimerHandle,
			this,
			&UCarRideComponent::OnBladderPulseFinished,
			BladderComp->PulseDuration,
			false
		);
	}
}

void UCarRideComponent::OnBladderPulseFinished()
{
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

	// Re-show the dialogue widget (Close() set it to Collapsed, UpdateWithText doesn't restore visibility)
	if (Rick && Rick->DialogueWidgetComponent)
	{
		if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
		{
			DialogueWidget->SetVisibility(ESlateVisibility::Visible);
		}
	}

	// Auto-advance to show the next line ("You need to pee?")
	Player->AdvanceDialogue();
	Player->SetCanInteract(true);
}

void UCarRideComponent::EndRide()
{
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

	// Show the gas station now that the ride is over
	if (GasStationRoot)
	{
		TArray<AActor*> GasStationActors;
		GasStationRoot->GetAttachedActors(GasStationActors, /*bResetArray=*/true, /*bRecursively=*/true);
		for (AActor* Actor : GasStationActors)
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
		}
	}

	// Restore all spawned movie boxes
	{
		TArray<AActor*> MovieBoxes;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AMovieBox::StaticClass(), MovieBoxes);
		for (AActor* Actor : MovieBoxes)
		{
			Actor->SetActorHiddenInGame(false);
			Actor->SetActorEnableCollision(true);
		}
	}

	// Re-enable collision on scenery
	if (SceneryRoot)
	{
		SceneryRoot->SetActorEnableCollision(true);
	}

	// Re-enable movement and gravity
	PC->SetIgnoreMoveInput(false);
	if (UCharacterMovementComponent* MoveComp = Player->GetCharacterMovement())
	{
		MoveComp->SetJumpAllowed(true);
		MoveComp->GravityScale = 1.0f;
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

	if (Rick)
	{
		// Restore widget to its original relative position and ensure it's closed
		if (Rick->DialogueWidgetComponent)
		{
			AActor* WidgetActor = Rick->DialogueWidgetComponent->GetOwner();
			if (WidgetActor && WidgetActor->GetRootComponent())
			{
				WidgetActor->GetRootComponent()->SetRelativeLocation(CachedWidgetRelativeLocation);
				WidgetActor->GetRootComponent()->SetRelativeRotation(CachedWidgetRelativeRotation);
			}
			if (UUI_Dialogue* DialogueWidget = Cast<UUI_Dialogue>(Rick->DialogueWidgetComponent->GetWidget()))
			{
				DialogueWidget->Close();
			}
		}
		Rick->AppearOutside();
	}
}
