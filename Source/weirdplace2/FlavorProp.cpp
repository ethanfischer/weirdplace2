#include "FlavorProp.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AFlavorProp::AFlavorProp()
{
}

bool AFlavorProp::CanInteract()
{
	const UWorld* World = GetWorld();
	return World && (LastInteractTime < 0.0 || World->GetTimeSeconds() - LastInteractTime >= InteractCooldown);
}

void AFlavorProp::Interact_Implementation()
{
	LastInteractTime = GetWorld()->GetTimeSeconds();

	if (InteractSounds.Num() > 0)
	{
		USoundBase* Sound = InteractSounds[FMath::RandRange(0, InteractSounds.Num() - 1)];
		if (Sound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
		}
	}

	UE_LOG(LogTemp, Log, TEXT("AFlavorProp %s: interacted"), *GetName());
	OnFlavorInteract();
}
