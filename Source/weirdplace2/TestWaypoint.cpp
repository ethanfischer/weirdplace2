#include "TestWaypoint.h"
#include "Components/BillboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ATestWaypoint::ATestWaypoint()
{
	PrimaryActorTick.bCanEverTick = false;

	Billboard = CreateDefaultSubobject<UBillboardComponent>(TEXT("Billboard"));
	RootComponent = Billboard;
}

ATestWaypoint* ATestWaypoint::FindByTag(const UObject* WorldContext, FName InTag)
{
	if (!WorldContext)
	{
		return nullptr;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ATestWaypoint> It(World); It; ++It)
	{
		if (It->Tag == InTag)
		{
			return *It;
		}
	}
	return nullptr;
}
