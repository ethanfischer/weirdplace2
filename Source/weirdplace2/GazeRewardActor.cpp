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

	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);

	const FVector ToMe = GetActorLocation() - CamLoc;
	const float Dist = ToMe.Size();
	if (Dist > MaxGazeDistance || Dist < KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float CosAngle = FVector::DotProduct(CamRot.Vector(), ToMe / Dist);
	if (CosAngle < FMath::Cos(FMath::DegreesToRadians(GazeConeHalfAngleDegrees)))
	{
		return false;
	}

	// Line of sight: this actor sits at the stared-at fixture's bounds center,
	// so a hit in the last 10% of the ray is the fixture itself. Anything
	// nearer is a real obstruction.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GazeReward), /*bTraceComplex*/ false);
	Params.AddIgnoredActor(PC->GetPawn());
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, GetActorLocation(), ECC_Visibility, Params)
		&& Hit.Distance < Dist * 0.9f)
	{
		return false;
	}

	return true;
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
