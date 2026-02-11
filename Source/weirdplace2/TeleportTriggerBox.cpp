#include "TeleportTriggerBox.h"
#include "GameFramework/Character.h"
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

	// Optionally destroy Ultra Dynamic Sky actors
	if (bDestroyUltraDynamicActors)
	{
		DestroyUltraDynamicActors();
	}

	// Teleport the actor
	OtherActor->SetActorTransform(TeleportTransform, false, nullptr, ETeleportType::TeleportPhysics);
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
