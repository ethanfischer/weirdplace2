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
#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "FirstPersonCharacter.h"
#include "Tunable.h"

WP_TUNABLE_FLOAT(GOasisExposureBlendSeconds, "weird.Oasis.ExposureBlendSeconds", 4.f,
	"Seconds over which the oasis teleport fades from Ultra Dynamic Sky's exposure to the oasis's own.");

namespace
{
	constexpr float ExposureHandOffStep = 1.f / 30.f;
}

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

	// Optionally destroy Ultra Dynamic Sky actors (its exposure is handed over
	// to the camera first so the brightness doesn't snap).
	if (bDestroyUltraDynamicActors)
	{
		HandOverSkyExposure(OtherActor);
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
		if (UAudioComponent* Waterfall = AmbientWaterfall ? AmbientWaterfall->GetAudioComponent() : nullptr)
		{
			Waterfall->AdjustVolume(0.0f, 1.0f);
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

void ATeleportTriggerBox::HandOverSkyExposure(AActor* Traveller)
{
	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(Traveller);
	UCameraComponent* Camera = Player ? Player->GetFirstPersonCamera() : nullptr;
	if (!Camera)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s: %s has no first-person camera; exposure will snap"), *GetName(), *GetNameSafe(Traveller));
		return;
	}

	const UPostProcessComponent* SkyPP = nullptr;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Sky")))
		{
			SkyPP = It->FindComponentByClass<UPostProcessComponent>();
			break;
		}
	}
	if (!SkyPP)
	{
		UE_LOG(LogTemp, Error, TEXT("TeleportTriggerBox %s: no Ultra_Dynamic_Sky post-process found; exposure will snap"), *GetName());
		return;
	}

	// Copy whichever auto-exposure settings the sky overrides.
	const FPostProcessSettings& Sky = SkyPP->Settings;
	FPostProcessSettings& Cam = Camera->PostProcessSettings;
	Cam.bOverride_AutoExposureMethod = Sky.bOverride_AutoExposureMethod;
	Cam.AutoExposureMethod = Sky.AutoExposureMethod;
	Cam.bOverride_AutoExposureBias = Sky.bOverride_AutoExposureBias;
	Cam.AutoExposureBias = Sky.AutoExposureBias;
	Cam.bOverride_AutoExposureMinBrightness = Sky.bOverride_AutoExposureMinBrightness;
	Cam.AutoExposureMinBrightness = Sky.AutoExposureMinBrightness;
	Cam.bOverride_AutoExposureMaxBrightness = Sky.bOverride_AutoExposureMaxBrightness;
	Cam.AutoExposureMaxBrightness = Sky.AutoExposureMaxBrightness;

	if (!ExposureCamera.IsValid())
	{
		ExposureCameraOriginalWeight = Camera->PostProcessBlendWeight;
	}
	ExposureCamera = Camera;
	Camera->PostProcessBlendWeight = ExposureCameraOriginalWeight;
	ExposureHandOffElapsed = 0.f;
	GetWorldTimerManager().SetTimer(ExposureHandOffTimer, this, &ATeleportTriggerBox::TickExposureHandOff, ExposureHandOffStep, true);
	UE_LOG(LogTemp, Log, TEXT("TeleportTriggerBox %s: handing sky exposure to the camera (bias %s%.2f, range %s[%.1f, %.1f]) over %.1fs"),
		*GetName(), Sky.bOverride_AutoExposureBias ? TEXT("") : TEXT("unset "), Sky.AutoExposureBias,
		Sky.bOverride_AutoExposureMinBrightness ? TEXT("") : TEXT("unset "), Sky.AutoExposureMinBrightness, Sky.AutoExposureMaxBrightness,
		GOasisExposureBlendSeconds);
}

void ATeleportTriggerBox::TickExposureHandOff()
{
	UCameraComponent* Camera = ExposureCamera.Get();
	if (!Camera)
	{
		GetWorldTimerManager().ClearTimer(ExposureHandOffTimer);
		return;
	}
	ExposureHandOffElapsed += ExposureHandOffStep;
	const float Alpha = FMath::Clamp(ExposureHandOffElapsed / FMath::Max(0.01f, GOasisExposureBlendSeconds), 0.f, 1.f);
	// The camera's settings blend in by PostProcessBlendWeight: fading it fades
	// the sky's exposure out into whatever the oasis volumes/defaults say.
	Camera->PostProcessBlendWeight = ExposureCameraOriginalWeight * (1.f - FMath::SmoothStep(0.f, 1.f, Alpha));
	if (Alpha >= 1.f)
	{
		FPostProcessSettings& Cam = Camera->PostProcessSettings;
		Cam.bOverride_AutoExposureMethod = false;
		Cam.bOverride_AutoExposureBias = false;
		Cam.bOverride_AutoExposureMinBrightness = false;
		Cam.bOverride_AutoExposureMaxBrightness = false;
		Camera->PostProcessBlendWeight = ExposureCameraOriginalWeight;
		ExposureCamera.Reset();
		GetWorldTimerManager().ClearTimer(ExposureHandOffTimer);
		UE_LOG(LogTemp, Log, TEXT("TeleportTriggerBox %s: exposure hand-off complete"), *GetName());
	}
}

float ATeleportTriggerBox::GetExposureHandOffRemaining() const
{
	return ExposureCamera.IsValid() ? FMath::Max(0.f, GOasisExposureBlendSeconds - ExposureHandOffElapsed) : 0.f;
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
