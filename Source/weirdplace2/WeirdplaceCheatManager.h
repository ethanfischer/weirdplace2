#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "WeirdplaceCheatManager.generated.h"

// Home for weirdplace2's dev/debug console commands. UE only instantiates a
// CheatManager when cheats are enabled (automatic in PIE standalone,
// non-shipping), so this is the canonical place for cheats — they don't belong
// on the player pawn. Installed via AWeirdplacePlayerController::CheatClass.
UCLASS()
class WEIRDPLACE2_API UWeirdplaceCheatManager : public UCheatManager
{
	GENERATED_BODY()

public:
	// Dev: jump to a story beat or car-ride stage, for this play AND every
	// later play (saved to the per-user GameUserSettings ini; automation runs
	// ignore it) until `SkipTo clear`.
	//   `SkipTo`             — list beats/stages and the current persisted one
	//   `SkipTo <beat>`      — apply now + persist
	//   `SkipTo clear`       — stop persisting (also: off / none)
	// Story beats set every UStorySubsystem flag up to that beat (running its
	// side effects) and frame the relevant actor. Case-insensitive, aliases:
	//   KeyBroke (Key) · TornadoWarning (TornadoWarningDisplayed / TV) ·
	//   Telephone (SeenTornadoWarning / PayPhone)
	// Car-ride stages need the ride to be running (applied automatically when
	// it starts if persisted):
	//   CarDialogue -> Rick's first line · CarLastLine -> his final line showing,
	//   advance it yourself · CarDrive -> dialogue done, car pulling away
	// RULE: there is one skip system. A new beat/stage must resolve here, persist,
	// and appear in the bare listing.
	UFUNCTION(Exec) void SkipTo(const FString& BeatName);

	// Dev: teleport Seneca to her smoking spot and start the smoking anim.
	// Doesn't touch CurrentState or other quest flags. Type `SkipToSmoking` in PIE console.
	UFUNCTION(Exec) void SkipToSmoking();

	// Dev: teleport the player in front of the keypad-locked door (the employee
	// bathroom), facing it, so you can immediately test code entry. Type
	// `SkipToKeypad` in the PIE console. The code is the door's KeypadCode (4729).
	UFUNCTION(Exec) void SkipToKeypad();

	// Dev: grant an item to the player inventory by short name. Looks up the
	// data asset at /Game/Inventory/DA_<Name>. e.g. `GiveItem Key`, `GiveItem BrokenKey`.
	UFUNCTION(Exec) void GiveItem(const FString& Name);

	// Dev: grant every UItemDefinition under /Game/Inventory and open the inventory.
	// Skips items already held, so repeated runs don't create duplicate slots.
	UFUNCTION(Exec) void GiveAll();
};
