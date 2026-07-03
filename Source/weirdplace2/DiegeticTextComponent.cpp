#include "DiegeticTextComponent.h"
#include "GameFramework/PlayerController.h"

UDiegeticTextComponent::UDiegeticTextComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Match the project's established diegetic look (CantCarryText): white,
	// centered, engine default RobotoDistanceField font + DefaultTextMaterialOpaque.
	SetHorizontalAlignment(EHTA_Center);
	SetVerticalAlignment(EVRTA_TextCenter);
	SetTextRenderColor(FColor::White);
	SetWorldSize(6.0f);
}

void UDiegeticTextComponent::OnVisibilityChanged()
{
	Super::OnVisibilityChanged();
	SetComponentTickEnabled(IsVisible() && bFacePlayer);
}

void UDiegeticTextComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bFacePlayer || !IsVisible())
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	if (!PC)
	{
		return;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	// The readable face points along +X, so a rotation whose forward points at
	// the camera makes the text readable (confirmed by the prior hand-rolled
	// billboard).
	FVector ToCam = CamLoc - GetComponentLocation();
	if (bYawOnly)
	{
		ToCam.Z = 0.f;
	}
	if (ToCam.IsNearlyZero())
	{
		return;
	}

	const FRotator Look = bYawOnly
		? FRotator(0.f, ToCam.Rotation().Yaw, 0.f)
		: ToCam.Rotation();
	SetWorldRotation(Look);
}
