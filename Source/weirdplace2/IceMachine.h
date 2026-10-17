#pragma once

#include "CoreMinimal.h"
#include "FlavorProp.h"
#include "IceMachine.generated.h"

class UItemDefinition;
class USoundBase;
class UStaticMeshComponent;
class AFirstPersonCharacter;

// The ice machine outside the gas station. Poking it is a flavor prop. Hitting
// it with the meat tenderizer (IInteractable::UseItem) knocks both padlocks off,
// swings the left door open and then hands the player the Money inside (same
// give-item notification as any other reward).
//
// The Fab mesh was one piece; /Game/Fab/IceMachineParts holds it split into a
// body (root mesh), two doors pivoted on their outer hinge edge, and two
// padlocks pivoted at their centres. Pivot positions below are in body space,
// front = +Y.
UCLASS()
class WEIRDPLACE2_API AIceMachine : public AFlavorProp
{
	GENERATED_BODY()

public:
	AIceMachine();

	virtual bool UseItem(FName ItemID) override;
	virtual bool CanInteract() override { return !bBrokenOpen && Super::CanInteract(); } // nothing left to poke once it's open
	virtual bool WouldReactToItem(FName ItemID) override { return ItemID == OpenerItemID && !bBrokenOpen && MoneyDef != nullptr; }

	bool IsBrokenOpen() const { return bBrokenOpen; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ice Machine")
	UStaticMeshComponent* DoorL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ice Machine")
	UStaticMeshComponent* DoorR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ice Machine")
	UStaticMeshComponent* PadlockL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ice Machine")
	UStaticMeshComponent* PadlockR;

	// Item that breaks it open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	FName OpenerItemID = FName("MeatTenderizer");

	// What's inside: given to the player once the door has swung open.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	UItemDefinition* MoneyDef = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine")
	USoundBase* BreakOpenSound = nullptr;

	// How far the left door swings (degrees about its hinge). The right door
	// stays shut; only its padlock comes off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine|Animation")
	float DoorOpenAngle = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine|Animation")
	float DoorOpenSeconds = 0.7f;

	// Doors start swinging this long after the padlocks drop.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine|Animation")
	float DoorDelaySeconds = 0.3f;

	// Padlocks tumble for this long, then vanish.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ice Machine|Animation")
	float PadlockFallSeconds = 0.9f;

private:
	bool bBrokenOpen = false;
	float OpenElapsed = -1.0f; // <0 = not animating

	FVector PadlockLStart, PadlockRStart;

	// The player who broke it open; gets the money when the door finishes opening.
	TWeakObjectPtr<AFirstPersonCharacter> Opener;
	void GiveMoney();

	// Timer-driven rather than Tick: level instances of this actor were saved
	// with the tick function disabled (it's serialized per instance), so a
	// timer is the reliable way to animate.
	FTimerHandle AnimTimer;
	static constexpr float AnimStep = 1.0f / 60.0f;
	void TickBreakOpen();
};
