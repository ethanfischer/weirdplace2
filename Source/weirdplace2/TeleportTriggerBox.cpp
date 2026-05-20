#include "TeleportTriggerBox.h"
#include "BladderUrgencyComponent.h"
#include "Door.h"
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
			if (ClassName.Contains(TEXT("UltraDynamic")))
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
