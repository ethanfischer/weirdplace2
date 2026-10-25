#include "TeleportTriggerBox.h"
#include "BladderUrgencyComponent.h"
#include "Door.h"
#include "StorySubsystem.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"
#include "TimerManager.h"

ATeleportTriggerBox::ATeleportTriggerBox()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ATeleportTriggerBox::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	// Only teleport characters/pawns
	if (!OtherActor || !OtherActor->IsA(APawn::StaticClass()))
	{
		return;
	}

	TeleportActor(OtherActor);
}

void ATeleportTriggerBox::TeleportActor(AActor* OtherActor)
{
	if (!TeleportTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s has no TeleportTarget set"), *GetName());
		return;
	}

	// Optionally destroy Ultra Dynamic Sky actors
	if (bDestroyUltraDynamicActors)
	{
		DestroyUltraDynamicActors();
	}

	// Compute rotation delta between trigger and target
	FQuat RotationDelta = FQuat(TeleportTarget->GetActorRotation()) * FQuat(GetActorRotation()).Inverse();

	// 1. Preserve relative position offset
	FVector Offset = OtherActor->GetActorLocation() - GetActorLocation();
	FVector RotatedOffset = RotationDelta.RotateVector(Offset);
	OtherActor->SetActorLocation(TeleportTarget->GetActorLocation() + RotatedOffset);

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		// 2. Preserve camera direction (rotated by the delta)
		if (AController* Controller = Character->GetController())
		{
			FRotator NewViewRot = (RotationDelta * FQuat(Controller->GetControlRotation())).Rotator();
			Controller->SetControlRotation(NewViewRot);
			Character->SetActorRotation(FRotator(0, NewViewRot.Yaw, 0));
		}

		// 3. Preserve movement velocity (rotated by the delta)
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->Velocity = RotationDelta.RotateVector(MoveComp->Velocity);
		}
	}

	if (bSilenceGlobalWind)
	{
		SilenceGlobalWindIfRequested();
		FadeInAmbient(AmbientWaterfall, WaterfallFadeInDuration, WaterfallFadeCurve);
		FadeInAmbient(AmbientChord, ChordFadeInDuration, ChordFadeCurve);
	}

	if (bStopBladderUrgency)
	{
		if (UBladderUrgencyComponent* Bladder = OtherActor->FindComponentByClass<UBladderUrgencyComponent>())
		{
			Bladder->StopUrgency();
		}
	}

	if (BathroomStallDoor)
	{
		GetWorldTimerManager().SetTimer(
			BathroomStallDoorUnlockTimerHandle,
			this,
			&ATeleportTriggerBox::UnlockBathroomStallDoor,
			BathroomStallDoorUnlockTime,
			false);
	}

	UStorySubsystem* Story = GetWorld()->GetSubsystem<UStorySubsystem>();
	if (!Story)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s: no UStorySubsystem, EnteredOasis not set"), *GetName());
		return;
	}
	Story->SetFlag(EStoryFlag::EnteredOasis, true);
}

void ATeleportTriggerBox::SkipTransition(AActor* Actor)
{
	if (!SkipToTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s: SkipTransition but no SkipToTarget set"), *GetName());
		return;
	}

	TeleportActor(Actor);

	Actor->SetActorLocation(SkipToTarget->GetActorLocation());
	if (ACharacter* Character = Cast<ACharacter>(Actor))
	{
		const FRotator Facing(0, SkipToTarget->GetActorRotation().Yaw, 0);
		Character->SetActorRotation(Facing);
		if (AController* Controller = Character->GetController())
		{
			Controller->SetControlRotation(Facing);
		}
		if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
		{
			MoveComp->Velocity = FVector::ZeroVector;
		}
	}

	// Finish the ambient cross-fade TeleportActor just started.
	if (bSilenceGlobalWind)
	{
		if (UAudioComponent* Wind = AmbientGlobalWind ? AmbientGlobalWind->GetAudioComponent() : nullptr)
		{
			Wind->Stop();
		}
		for (AAmbientSound* Ambient : { AmbientWaterfall, AmbientChord })
		{
			if (UAudioComponent* AudioComp = Ambient ? Ambient->GetAudioComponent() : nullptr)
			{
				AudioComp->AdjustVolume(0.0f, 1.0f);
			}
		}
	}

	if (BathroomStallDoor)
	{
		GetWorldTimerManager().ClearTimer(BathroomStallDoorUnlockTimerHandle);
		UnlockBathroomStallDoor();
	}

	UE_LOG(LogTemp, Log, TEXT("TeleportTriggerBox %s: SkipTransition -> %s at %s"),
		*GetName(), *SkipToTarget->GetActorNameOrLabel(), *SkipToTarget->GetActorLocation().ToString());
}

void ATeleportTriggerBox::UnlockBathroomStallDoor()
{
	if (!BathroomStallDoor)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s: BathroomStallDoor went null before unlock timer fired"), *GetName());
		return;
	}

	BathroomStallDoor->SetLocked(false);
}

void ATeleportTriggerBox::FadeInAmbient(AAmbientSound* Ambient, float Duration, EAudioFaderCurve FadeCurve)
{
	if (!Ambient)
	{
		UE_LOG(LogTemp, Warning, TEXT("TeleportTriggerBox %s: FadeInAmbient called with null reference"), *GetName());
		return;
	}

	if (UAudioComponent* AudioComp = Ambient->GetAudioComponent())
	{
		AudioComp->FadeIn(Duration, 1.0f, 0.0f, FadeCurve);
	}
}

void ATeleportTriggerBox::SilenceGlobalWindIfRequested()
{
	if (!AmbientGlobalWind)
	{
		UE_LOG(LogTemp, Warning, TEXT("TeleportTriggerBox %s: bSilenceGlobalWind set but AmbientGlobalWind is unassigned"), *GetName());
		return;
	}

	if (UAudioComponent* AudioComp = AmbientGlobalWind->GetAudioComponent())
	{
		AudioComp->FadeOut(WindFadeOutDuration, 0.0f);
	}
}

void ATeleportTriggerBox::DestroyUltraDynamicActors()
{
	// Find and destroy Ultra Dynamic Sky actors by class name to avoid hard dependency
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	TArray<AActor*> ActorsToDestroy;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			FString ClassName = Actor->GetClass()->GetName();
			if (ClassName.Contains(TEXT("Ultra_Dynamic")))
			{
				ActorsToDestroy.Add(Actor);
			}
		}
	}

	for (AActor* Actor : ActorsToDestroy)
	{
		Actor->Destroy();
	}
}
