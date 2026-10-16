#pragma once

#include "CoreMinimal.h"
#include "FlavorProp.h"
#include "IceMachine.generated.h"

class UItemDefinition;
class USoundBase;
class AInspectablePickup;

// The ice machine outside the gas station. Poking it is a flavor prop; hitting
// it with the meat tenderizer (IInteractable::UseItem) breaks it open once and
// drops a Money pickup in front of it.
UCLASS()
class WEIRDPLACE2_API AIceMachine : public AFlavorProp
{
	GENERATED_BODY()

public:
	AIceMachine();

	virtual bool UseItem(FName ItemID) override;

	bool IsBrokenOpen() const { return bBrokenOpen; }

	// Item that breaks it open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	FName OpenerItemID = FName("MeatTenderizer");

	// What falls out. Spawned as an AInspectablePickup at MoneyOffset.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	UItemDefinition* MoneyDef = nullptr;

	// Local-space offset of the dropped money (actor forward is +X).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	FVector MoneyOffset = FVector(110.0f, 0.0f, 20.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	FRotator MoneyRotation = FRotator(0.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	USoundBase* BreakOpenSound = nullptr;

private:
	bool bBrokenOpen = false;

	UPROPERTY()
	AInspectablePickup* MoneyPickup = nullptr;
};
