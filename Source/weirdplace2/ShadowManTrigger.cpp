#include "ShadowManTrigger.h"
#include "ShadowMan.h"
#include "FirstPersonCharacter.h"

void AShadowManTrigger::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	if (!Cast<AFirstPersonCharacter>(OtherActor))
	{
		return;
	}

	if (!ShadowMan)
	{
		UE_LOG(LogTemp, Error, TEXT("ShadowManTrigger %s has no ShadowMan reference set"), *GetName());
		return;
	}

	ShadowMan->StartHaunting();
}

void AShadowManTrigger::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	if (!Cast<AFirstPersonCharacter>(OtherActor))
	{
		return;
	}

	if (!ShadowMan)
	{
		return;
	}

	ShadowMan->StopHaunting();
}
