#include "ShadowMan.h"
#include "CarRideComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

namespace
{
	// Far-below-world hide position (same idiom as the MetaHuman-hide feedback memory).
	constexpr double ShadowManHiddenZ = -100000.0;
}

AShadowMan::AShadowMan()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	BodyMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BodyMesh"));
	RootComponent = BodyMesh;
}

void AShadowMan::BeginPlay()
{
	Super::BeginPlay();

	// Start hidden offscreen so the silhouette doesn't render in the level until
	// the player enters the gas-station trigger.
	SetMeshVisible(false);
}

void AShadowMan::StartHaunting()
{
	if (bHaunting)
	{
		return;
	}

	bHaunting = true;
	StareElapsed = 0.f;
	TeleportBehindPlayer();
	SetMeshVisible(true);
	SetActorTickEnabled(true);
	UE_LOG(LogTemp, Display, TEXT("ShadowMan::StartHaunting"));
}

void AShadowMan::StopHaunting()
{
	if (!bHaunting)
	{
		return;
	}

	bHaunting = false;
	StareElapsed = 0.f;
	SetMeshVisible(false);
	SetActorTickEnabled(false);
	UE_LOG(LogTemp, Display, TEXT("ShadowMan::StopHaunting"));
}

void AShadowMan::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bHaunting)
	{
		return;
	}

	if (IsPlayerLookingAtMe())
	{
		StareElapsed += DeltaTime;
		if (StareElapsed >= StareTimeUntilKill)
		{
			TriggerKill();
			bHaunting = false;
		}
	}
	else
	{
		StareElapsed = 0.f;
	}
}

bool AShadowMan::IsPlayerLookingAt(const FVector& Position) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);

	FVector ToTarget = (Position - CameraLoc).GetSafeNormal();
	FVector CameraForward = CameraRot.Vector();

	float Dot = FVector::DotProduct(CameraForward, ToTarget);
	// ~60 degree half-angle cone
	return Dot > 0.5f;
}

bool AShadowMan::IsPlayerLookingAtMe() const
{
	FVector Center;
	if (BodyMesh)
	{
		FBoxSphereBounds LocalBounds = BodyMesh->GetLocalBounds();
		const FVector LocalUpperCenter = LocalBounds.Origin + FVector(0.f, 0.f, LocalBounds.BoxExtent.Z);
		Center = BodyMesh->GetComponentTransform().TransformPosition(LocalUpperCenter);
	}
	else
	{
		Center = GetActorLocation() + FVector(0.f, 0.f, 90.f);
	}
	return IsPlayerLookingAt(Center);
}

void AShadowMan::TriggerKill()
{
	UE_LOG(LogTemp, Display, TEXT("ShadowMan::TriggerKill - replaying car ride"));
	if (!CarRideOwner)
	{
		UE_LOG(LogTemp, Error, TEXT("ShadowMan::TriggerKill - CarRideOwner reference is null"));
		return;
	}
	UCarRideComponent* CarRide = CarRideOwner->FindComponentByClass<UCarRideComponent>();
	if (!CarRide)
	{
		UE_LOG(LogTemp, Error, TEXT("ShadowMan::TriggerKill - %s has no UCarRideComponent"), *CarRideOwner->GetName());
		return;
	}
	CarRide->RestartRide();
}

void AShadowMan::TeleportBehindPlayer()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		UE_LOG(LogTemp, Error, TEXT("ShadowMan::TeleportBehindPlayer - No PlayerController"));
		return;
	}

	FVector CameraLoc;
	FRotator CameraRot;
	PC->GetPlayerViewPoint(CameraLoc, CameraRot);

	const FVector CameraForward = CameraRot.Vector();
	const FVector SpawnLoc = CameraLoc - CameraForward * SpawnDistanceBehindPlayer;
	const FRotator FaceCameraRot(0.f, (CameraLoc - SpawnLoc).Rotation().Yaw, 0.f);

	SetActorLocationAndRotation(SpawnLoc, FaceCameraRot);
}

void AShadowMan::SetMeshVisible(bool bVisible)
{
	// Teleport-offscreen + collision-off pattern — toggling root visibility on a
	// MetaHuman breaks the groom hair on re-show (memory: feedback_metahuman_hide).
	if (!bVisible)
	{
		SetActorLocation(FVector(0.0, 0.0, ShadowManHiddenZ));
		SetActorEnableCollision(false);
	}
	else
	{
		SetActorEnableCollision(true);
	}
}
