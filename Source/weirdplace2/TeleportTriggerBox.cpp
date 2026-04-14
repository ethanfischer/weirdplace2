#include "TeleportTriggerBox.h"
#include "Engine/TargetPoint.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

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
