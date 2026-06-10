#include "GazeRewardActor.h"
#include "Components/AudioComponent.h"
#include "FirstPersonCharacter.h"
#include "Inventory.h"
#include "ItemDefinition.h"
#include "Kismet/GameplayStatics.h"

AGazeRewardActor::AGazeRewardActor()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	HumComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("Hum"));
	HumComponent->SetupAttachment(RootComponent);
	HumComponent->bAutoActivate = false;

	// Found by the E2E test driver (GetGazeHumState)
	Tags.Add(FName("GazeReward"));
}

void AGazeRewardActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bRewardGranted)
	{
		return;
	}

	if (!IsPlayerGazingAtMe())
	{
		if (GazeSeconds > 0.f)
		{
			GazeSeconds = 0.f;
			HumComponent->Stop();
		}
		return;
	}

	GazeSeconds += DeltaTime;

	if (HumSound)
	{
		if (!HumComponent->IsPlaying())
		{
			HumComponent->SetSound(HumSound);
			HumComponent->Play();
		}
		// Swell toward the payout; 0.05 floor so it's faintly audible from the
		// first held frame.
		HumComponent->SetVolumeMultiplier(FMath::Max(0.05f, GazeSeconds / RequiredLookSeconds));
	}

	if (GazeSeconds >= RequiredLookSeconds)
	{
		GrantReward();
	}
}

bool AGazeRewardActor::IsPlayerGazingAtMe() const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return false;
	}

	if (!GazeTarget)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardActor '%s': GazeTarget not set"), *GetName());
		return false;
	}

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	const FVector Dir = CamRot.Vector();

	// Gaze = the camera's center ray crosses the target's bounding box — ANY
	// point of the fixture counts (the canopy bar is 30m long; a cone around
	// its center point only ever worked for the test driver, not a human).
	// Box test instead of a hit test because light fixtures may have no
	// collision at all. Slab method, tracking the ray's box-entry distance.
	const FBox TargetBox = GazeTarget->GetComponentsBoundingBox(/*bNonColliding*/ true).ExpandBy(10.f);
	float TMin = 0.f;
	float TMax = MaxGazeDistance;
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

	// Occlusion: anything solid between the camera and where the ray enters
	// the fixture's box blocks the gaze. The target itself is ignored so this
	// works whether or not the fixture has collision.
	const FVector EntryPoint = CamLoc + Dir * TMin;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GazeReward), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(PC->GetPawn());
	Params.AddIgnoredActor(GazeTarget);
	return !GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, EntryPoint, ECC_Visibility, Params);
}

void AGazeRewardActor::GrantReward()
{
	// All failure paths below are misconfiguration: log and stop ticking.
	SetActorTickEnabled(false);

	if (!RewardItem || RewardItem->ItemID.IsNone())
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardActor '%s': RewardItem missing or has no ItemID"), *GetName());
		return;
	}

	AFirstPersonCharacter* Player = Cast<AFirstPersonCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Player)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardActor '%s': no AFirstPersonCharacter player"), *GetName());
		return;
	}

	UInventoryComponent* Inventory = Player->GetInventoryComponent();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Error, TEXT("GazeRewardActor '%s': player has no InventoryComponent"), *GetName());
		return;
	}

	const FInventoryItemData ItemData = RewardItem->ToInventoryItemData();
	Inventory->AddItemWithData(ItemData);
	Player->ShowItemNotification(ItemData, RewardItem->NotificationRotation);

	bRewardGranted = true;
	HumComponent->Stop();
	UE_LOG(LogTemp, Log, TEXT("GazeRewardActor '%s': granted '%s' after %.1fs gaze"),
		*GetName(), *RewardItem->ItemID.ToString(), GazeSeconds);
}
