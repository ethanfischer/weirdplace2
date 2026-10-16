#include "IceMachine.h"
#include "InspectablePickup.h"
#include "ItemDefinition.h"
#include "Kismet/GameplayStatics.h"

AIceMachine::AIceMachine()
{
}

bool AIceMachine::UseItem(FName ItemID)
{
	if (ItemID != OpenerItemID || bBrokenOpen)
	{
		return false;
	}
	if (!MoneyDef)
	{
		UE_LOG(LogTemp, Error, TEXT("IceMachine %s: MoneyDef not set; cannot drop money"), *GetName());
		return false;
	}

	bBrokenOpen = true;
	if (BreakOpenSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakOpenSound, GetActorLocation());
	}

	const FTransform DropTransform(
		GetActorRotation() + MoneyRotation,
		GetActorTransform().TransformPosition(MoneyOffset));
	MoneyPickup = GetWorld()->SpawnActorDeferred<AInspectablePickup>(
		AInspectablePickup::StaticClass(), DropTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!MoneyPickup)
	{
		UE_LOG(LogTemp, Error, TEXT("IceMachine %s: failed to spawn money pickup"), *GetName());
		return true;
	}
	MoneyPickup->SetItemDef(MoneyDef);
	UGameplayStatics::FinishSpawningActor(MoneyPickup, DropTransform);
	UE_LOG(LogTemp, Log, TEXT("IceMachine %s: broken open with %s, money at %s"),
		*GetName(), *ItemID.ToString(), *DropTransform.GetLocation().ToString());
	return true;
}
