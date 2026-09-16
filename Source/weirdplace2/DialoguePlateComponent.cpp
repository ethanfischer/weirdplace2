#include "DialoguePlateComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Tunable.h"

// Viewport band (0..1 fractions) the plate center is kept inside while the NPC
// is in view.
WP_TUNABLE_FLOAT(GDialoguePlateSafeMinFrac, "weird.DialoguePlate.SafeMinFrac", 0.15f,
	"Lower viewport fraction (both axes) the dialogue plate center is clamped to.");
WP_TUNABLE_FLOAT(GDialoguePlateSafeMaxFrac, "weird.DialoguePlate.SafeMaxFrac", 0.85f,
	"Upper viewport fraction (both axes) the dialogue plate center is clamped to.");
// While clamped the plate is pulled toward the camera so it floats in front of
// the NPC instead of intersecting them, and scaled so it reads at the size it
// has when the anchor is ReferenceDistance away (conversational range).
WP_TUNABLE_FLOAT(GDialoguePlateMinCameraDistance, "weird.DialoguePlate.MinCameraDistance", 60.f,
	"Camera distance (cm) the dialogue plate sits at while clamped, if the anchor is farther.");
WP_TUNABLE_FLOAT(GDialoguePlateReferenceDistance, "weird.DialoguePlate.ReferenceDistance", 150.f,
	"Anchor distance (cm) whose apparent plate size is preserved while clamped.");
WP_TUNABLE_FLOAT(GDialoguePlateInterpSpeed, "weird.DialoguePlate.InterpSpeed", 10.f,
	"Interp speed for the dialogue plate moving between anchor and clamped position.");
// Clamp only engages while the NPC body is inside this cone around the camera
// forward; otherwise the plate rests at its anchor rather than chasing the view.
WP_TUNABLE_FLOAT(GDialoguePlateConeHalfAngleDeg, "weird.DialoguePlate.ConeHalfAngleDeg", 60.f,
	"Half-angle (deg) of the view cone the NPC must be inside for plate clamping to engage.");
WP_TUNABLE_BOOL(GDialoguePlateDebug, "weird.DialoguePlate.Debug", false,
	"Log dialogue plate clamp state every tick.");

UDialoguePlateComponent::UDialoguePlateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDialoguePlateComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	TArray<UChildActorComponent*> ChildActorComponents;
	Owner->GetComponents<UChildActorComponent>(ChildActorComponents);
	for (UChildActorComponent* ChildActorComp : ChildActorComponents)
	{
		if (!ChildActorComp->GetName().Contains(TEXT("WorldSpace_UI_Dialogue")))
		{
			continue;
		}
		if (AActor* ChildActor = ChildActorComp->GetChildActor())
		{
			WidgetComponent = ChildActor->FindComponentByClass<UWidgetComponent>();
		}
		break;
	}

	if (!WidgetComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("DialoguePlateComponent[%s] - no WorldSpace_UI_Dialogue widget component found"), *Owner->GetName());
		SetComponentTickEnabled(false);
		return;
	}
	if (!WidgetComponent->GetAttachParent())
	{
		UE_LOG(LogTemp, Error, TEXT("DialoguePlateComponent[%s] - widget component has no attach parent; cannot anchor"), *Owner->GetName());
		SetComponentTickEnabled(false);
		return;
	}

	// A semi-transparent dialogue backing plate (UUI_Dialogue) needs alpha
	// blending. Force Transparent blend so BackingOpacity is a true gradient --
	// Masked blend clips it binary at the 0.333 mask threshold, which reads as
	// a hard step near ~0.35.
	WidgetComponent->SetBlendMode(EWidgetBlendMode::Transparent);

	// The view-cone test needs the rendered body, not the actor root (which sits
	// at the feet) or the whole-actor bounds (skewed by far-off components).
	BodyMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
	if (!BodyMesh)
	{
		UE_LOG(LogTemp, Error, TEXT("DialoguePlateComponent[%s] - no skeletal mesh to test the view cone against"), *Owner->GetName());
		SetComponentTickEnabled(false);
		return;
	}

	AnchorRelativeLocation = WidgetComponent->GetRelativeLocation();
	BaseRelativeScale = WidgetComponent->GetRelativeScale3D();
	BaseWorldScale = WidgetComponent->GetComponentScale();
}

FVector UDialoguePlateComponent::GetAnchorWorldLocation() const
{
	if (!WidgetComponent || !WidgetComponent->GetAttachParent())
	{
		return FVector::ZeroVector;
	}
	return WidgetComponent->GetAttachParent()->GetComponentTransform().TransformPosition(AnchorRelativeLocation);
}

void UDialoguePlateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}

	FVector CamLocation;
	FRotator CamRotation;
	PC->GetPlayerViewPoint(CamLocation, CamRotation);

	const FVector Anchor = GetAnchorWorldLocation();
	FVector TargetLocation = Anchor;
	FVector TargetScale = BaseRelativeScale;

	// Clamp only while the NPC body is in the view cone.
	const FVector NpcCenter = BodyMesh->Bounds.Origin;
	const FVector CamForward = CamRotation.Vector();
	const float ConeDot = FVector::DotProduct(CamForward, (NpcCenter - CamLocation).GetSafeNormal());
	const bool bNpcInCone = ConeDot > FMath::Cos(FMath::DegreesToRadians(GDialoguePlateConeHalfAngleDeg));

	int32 SizeX = 0, SizeY = 0;
	PC->GetViewportSize(SizeX, SizeY);
	FVector2D AnchorScreen;
	if (GDialoguePlateDebug)
	{
		UE_LOG(LogTemp, Log, TEXT("DialoguePlate[%s] tick cone=%.2f inCone=%d view=%dx%d"), *GetOwner()->GetName(), ConeDot, bNpcInCone, SizeX, SizeY);
	}
	if (bNpcInCone && SizeX > 0 && SizeY > 0 && PC->ProjectWorldLocationToScreen(Anchor, AnchorScreen, false))
	{
		// Projected half-extents (viewport fractions) of the draw rect at a given
		// camera distance and relative scale.
		const float HalfFovH = FMath::DegreesToRadians(PC->PlayerCameraManager->GetFOVAngle() * 0.5f);
		const float TanH = FMath::Tan(HalfFovH);
		const float TanV = TanH * SizeY / SizeX;
		const FVector2D DrawSize = WidgetComponent->GetDrawSize();
		// ScaleMul is the multiplier applied to the authored (base) scale.
		auto HalfExtentFrac = [&](float Dist, float ScaleMul)
		{
			return FVector2D(DrawSize.X * BaseWorldScale.Y * ScaleMul / (4.f * Dist * TanH),
				DrawSize.Y * BaseWorldScale.Z * ScaleMul / (4.f * Dist * TanV));
		};

		const float AnchorDist = FVector::Dist(Anchor, CamLocation);
		const FVector2D Frac(AnchorScreen.X / SizeX, AnchorScreen.Y / SizeY);
		const FVector2D AnchorHalf = HalfExtentFrac(AnchorDist, 1.f);

		// Rest at the anchor if the center is inside the safe band and the whole
		// rect is on screen.
		const bool bCenterInBand = Frac.X >= GDialoguePlateSafeMinFrac && Frac.X <= GDialoguePlateSafeMaxFrac
			&& Frac.Y >= GDialoguePlateSafeMinFrac && Frac.Y <= GDialoguePlateSafeMaxFrac;
		const bool bRectOnScreen = Frac.X - AnchorHalf.X >= 0.f && Frac.X + AnchorHalf.X <= 1.f
			&& Frac.Y - AnchorHalf.Y >= 0.f && Frac.Y + AnchorHalf.Y <= 1.f;
		if (GDialoguePlateDebug)
		{
			UE_LOG(LogTemp, Log, TEXT("DialoguePlate[%s] frac=(%.3f,%.3f) half=(%.3f,%.3f) dist=%.1f fov=%.1f draw=(%.0f,%.0f) scale=%s cone=%.2f band=%d onscreen=%d"),
				*GetOwner()->GetName(), Frac.X, Frac.Y, AnchorHalf.X, AnchorHalf.Y, AnchorDist, PC->PlayerCameraManager->GetFOVAngle(),
				DrawSize.X, DrawSize.Y, *BaseWorldScale.ToString(), ConeDot, bCenterInBand, bRectOnScreen);
		}
		if (!bCenterInBand || !bRectOnScreen)
		{
			const float NewDist = FMath::Min(AnchorDist, GDialoguePlateMinCameraDistance);
			const float ScaleMul = NewDist / GDialoguePlateReferenceDistance;
			TargetScale = BaseRelativeScale * ScaleMul;
			const FVector2D Half = HalfExtentFrac(NewDist, ScaleMul);

			// Center band intersected with "rect on screen". If the rect is taller
			// than that allows, keep its top on screen (the text sits at the top).
			auto ClampAxis = [](float Value, float HalfExtent)
			{
				const float Lo = FMath::Max(GDialoguePlateSafeMinFrac, HalfExtent);
				const float Hi = FMath::Min(GDialoguePlateSafeMaxFrac, 1.f - HalfExtent);
				return Lo <= Hi ? FMath::Clamp(Value, Lo, Hi) : Lo;
			};
			const FVector2D Clamped(ClampAxis(Frac.X, Half.X), ClampAxis(Frac.Y, Half.Y));

			FVector RayOrigin, RayDir;
			if (PC->DeprojectScreenPositionToWorld(Clamped.X * SizeX, Clamped.Y * SizeY, RayOrigin, RayDir))
			{
				TargetLocation = RayOrigin + RayDir * NewDist;
			}
		}
	}

	const FVector NewLocation = FMath::VInterpTo(WidgetComponent->GetComponentLocation(), TargetLocation, DeltaTime, GDialoguePlateInterpSpeed);
	const FVector NewScale = FMath::VInterpTo(WidgetComponent->GetRelativeScale3D(), TargetScale, DeltaTime, GDialoguePlateInterpSpeed);
	WidgetComponent->SetWorldLocation(NewLocation);
	WidgetComponent->SetRelativeScale3D(NewScale);

	// Billboard toward the camera.
	WidgetComponent->SetWorldRotation((CamLocation - NewLocation).Rotation());
}
