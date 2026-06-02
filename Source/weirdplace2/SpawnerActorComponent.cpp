// Fill out your copyright notice in the Description page of Project Settings.


#include "SpawnerActorComponent.h"

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

	if (ChordAmbientSound && TopShelfMovieBoxes.Num() > 0)
	{
		const int32 RandomIndex = FMath::RandRange(0, TopShelfMovieBoxes.Num() - 1);
		ChosenBox = TopShelfMovieBoxes[RandomIndex];

		const FVector OffsetLocation = ChosenBox->GetActorLocation() + ChosenBox->GetActorForwardVector() * ChosenForwardOffset;
		ChosenBox->SetActorLocation(OffsetLocation);

		ChordAmbientSound->SetActorLocation(OffsetLocation);
		if (UAudioComponent* AudioComp = ChordAmbientSound->GetAudioComponent())
		{
			AudioComp->SetVolumeMultiplier(CurrentVolumeMultiplier);
		}
		UE_LOG(LogTemp, Log, TEXT("SpawnerActorComponent: ChosenBox %s offset by %.1fcm forward; ChordAmbientSound positioned at it"), *ChosenBox->GetName(), ChosenForwardOffset);
	}
	else if (!ChordAmbientSound)
	{
		UE_LOG(LogTemp, Warning, TEXT("SpawnerActorComponent: ChordAmbientSound not assigned on %s — skipping random placement"), *Owner->GetName());
	}
}