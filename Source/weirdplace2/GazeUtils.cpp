#include "GazeUtils.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"

bool UGazeUtils::IsActorInPlayerGaze(const AActor* Target, UWorld* World,
	float BoxExpand, float MaxDistance, bool bRequireLineOfSight)
{
	if (!World || !Target)
	{
		return false;
	}
	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Dir = CamRot.Vector();

	// Gaze = the camera's center ray crosses the target's bounding box — ANY
	// point of it counts. Slab method, tracking the ray's box-entry distance.
	const FBox TargetBox = Target->GetComponentsBoundingBox(/*bNonColliding*/ true).ExpandBy(BoxExpand);
	float TMin = 0.f;
	float TMax = MaxDistance;
	for (int32 i = 0; i < 3; ++i)
	{
		if (FMath::IsNearlyZero(Dir[i]))
		{
			if (CamLoc[i] < TargetBox.Min[i] || CamLoc[i] > TargetBox.Max[i])
			{
				return false;
			}
		}
		else
		{
			float T1 = (TargetBox.Min[i] - CamLoc[i]) / Dir[i];
			float T2 = (TargetBox.Max[i] - CamLoc[i]) / Dir[i];
			if (T1 > T2)
			{
				Swap(T1, T2);
			}
			TMin = FMath::Max(TMin, T1);
			TMax = FMath::Min(TMax, T2);
			if (TMin > TMax)
			{
				return false;
			}
		}
	}

	if (!bRequireLineOfSight)
	{
		return true;
	}

	// Occlusion: anything solid between the camera and where the ray enters the
	// target's box blocks the gaze. Target + player are ignored so this works
	// whether or not the target has collision.
	const FVector EntryPoint = CamLoc + Dir * TMin;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GazeUtils), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(PC->GetPawn());
	Params.AddIgnoredActor(Target);
	return !World->LineTraceSingleByChannel(Hit, CamLoc, EntryPoint, ECC_Visibility, Params);
}
