// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnerActorComponent.h"

#include "MovieBox.h"
#include "Sound/AmbientSound.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

// Sets default values for this component's properties
USpawnerActorComponent::USpawnerActorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void USpawnerActorComponent::BeginPlay()
{
	Super::BeginPlay();

	Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpawnManagerComponent::SpawnMultiple - No Owner Found"));
	}

	World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpawnManagerComponent::SpawnMultiple - No World Found"));
	}

	SpawnMovieBoxes();
}

void USpawnerActorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ChosenBox || !ChordAmbientSound)
	{
		return;
	}

	UAudioComponent* AudioComp = ChordAmbientSound->GetAudioComponent();
	if (!AudioComp)
	{
		return;
	}

	if (AMovieBox* ChosenMovieBox = Cast<AMovieBox>(ChosenBox))
	{
		if (ChosenMovieBox->WasCollected())
		{
			UE_LOG(LogTemp, Log, TEXT("SpawnerActorComponent: ChosenBox %s collected, stopping chord"), *ChosenBox->GetName());
			AudioComp->Stop();
			ChosenBox = nullptr;
			return;
		}
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	PC->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * 10000.0f;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ChosenBoxReticle), false);
	Params.AddIgnoredActor(GetOwner());
	const bool bHit = World->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params);
	const bool bLookingAtChosen = bHit && Hit.GetActor() == ChosenBox;

	const float TargetMultiplier = bLookingAtChosen ? LookAtVolumeMultiplier : 1.0f;
	CurrentVolumeMultiplier = FMath::FInterpConstantTo(CurrentVolumeMultiplier, TargetMultiplier, DeltaTime, LookAtFadeSpeed);
	AudioComp->SetVolumeMultiplier(CurrentVolumeMultiplier);
}

void USpawnerActorComponent::SpawnMovieBoxes()
{
	const auto SpawnerLocation = Owner->GetActorLocation();
	const auto SpawnerRotation = Owner->GetActorRotation();
	const auto SpawnerRotationFlipped = FRotator(SpawnerRotation.Pitch, SpawnerRotation.Yaw + 180, SpawnerRotation.Roll);
	if (!DataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnerActorComponent: No DataTable assigned!"));
		return;
	}

	const auto VideoNames = DataTable->GetRowNames();
	if (VideoNames.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnerActorComponent: DataTable is empty!"));
		return;
	}

	auto       VideoNameIndex = 0;

	TArray<AActor*> TopShelfMovieBoxes;

	int32 TopShelfIndex = 0;
	for (auto i = 1; i < ShelfLocations.Num(); i++)
	{
		if (ShelfLocations[i].Z > ShelfLocations[TopShelfIndex].Z)
		{
			TopShelfIndex = i;
		}
	}

	for (auto ShelfIndex = 0; ShelfIndex < ShelfLocations.Num(); ShelfIndex++)
	{
		for (auto BookcaseIndex = 0; BookcaseIndex < BookcaseLocations.Num(); BookcaseIndex++)
		{
			for (auto i = 0; i < AmountPerShelf; i++)
			{
				auto AdjustedLocation = SpawnerLocation + (i * Spacing * SpawnDirection + (BookcaseLocations[BookcaseIndex] + ShelfLocations[ShelfIndex]));
				auto AdjustedRotation = BookcaseIndex % 2 == 0 ? SpawnerRotation : SpawnerRotationFlipped;
				FActorSpawnParameters SpawnParameters;
				FName BaseName = VideoNames[VideoNameIndex % VideoNames.Num()];
				SpawnParameters.Name = FName(*FString::Printf(TEXT("%s_%d"), *BaseName.ToString(), VideoNameIndex));
				AActor* Spawned = World->SpawnActor<AActor>(MovieBoxClass, AdjustedLocation, AdjustedRotation, SpawnParameters);
				if (Spawned && ShelfIndex == TopShelfIndex)
				{
					TopShelfMovieBoxes.Add(Spawned);
				}
				VideoNameIndex++;
			}
		}
	}

	if (!BlankVhsBoxClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnerActorComponent: BlankVhsBoxClass not assigned on %s — skipping chosen-tape replacement and chord"), *Owner->GetName());
		return;
	}

	if (TopShelfMovieBoxes.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnerActorComponent: TopShelfMovieBoxes is empty on %s — cannot pick chosen tape"), *Owner->GetName());
		return;
	}

	const int32 RandomIndex = FMath::RandRange(0, TopShelfMovieBoxes.Num() - 1);
	AActor* OriginalBox = TopShelfMovieBoxes[RandomIndex];
	const FTransform OriginalTransform = OriginalBox->GetActorTransform();
	OriginalBox->Destroy();

	FActorSpawnParameters ReplaceParams;
	ReplaceParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ChosenBox = World->SpawnActor<AMovieBox>(BlankVhsBoxClass, OriginalTransform.GetLocation(), OriginalTransform.Rotator(), ReplaceParams);
	if (!ChosenBox)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnerActorComponent: Failed to spawn BlankVhsBoxClass replacement"));
		return;
	}

	const FVector OffsetLocation = ChosenBox->GetActorLocation() + ChosenBox->GetActorForwardVector() * ChosenForwardOffset;
	ChosenBox->SetActorLocation(OffsetLocation);

	ChosenItemID = BlankVhsBoxClass->GetDefaultObject<AMovieBox>()->ItemIDOverride;

	if (ChordAmbientSound)
	{
		ChordAmbientSound->SetActorLocation(OffsetLocation);
		if (UAudioComponent* AudioComp = ChordAmbientSound->GetAudioComponent())
		{
			AudioComp->SetVolumeMultiplier(CurrentVolumeMultiplier);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnerActorComponent: ChordAmbientSound not assigned on %s — chord disabled but chosen tape still placed"), *Owner->GetName());
	}

	UE_LOG(LogTemp, Log, TEXT("SpawnerActorComponent: ChosenBox %s (ChosenItemID=%s) offset by %.1fcm forward"), *ChosenBox->GetName(), *ChosenItemID.ToString(), ChosenForwardOffset);
}