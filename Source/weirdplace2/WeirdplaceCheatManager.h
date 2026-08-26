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
	// Dev: jump the tornado/telephone story to a beat and teleport to look at it.
	// Sets every UStorySubsystem flag up to that beat (running its side effects),
	// then frames the relevant actor. Beats (case-insensitive, with aliases):
	//   `SkipTo KeyBroke` (or `Key`)
	//   `SkipTo TornadoWarning` (or `TornadoWarningDisplayed` / `TV`)  -> both store TVs flip
	//   `SkipTo Telephone` (or `SeenTornadoWarning` / `PayPhone`)      -> pole + pay phone reveal
	// With no argument, lists one command per story beat.
	UFUNCTION(Exec) void SkipTo(const FString& BeatName);

	// Dev: persist a beat that auto-applies at the start of every play session
	// (saved to the per-user GameUserSettings ini; automation runs ignore it).
	//   `AutoSkip Telephone` — set (same beat names/aliases as SkipTo)
	//   `AutoSkip`           — show the current setting
	//   `AutoSkip clear`     — remove it (also: off / none)
	UFUNCTION(Exec) void AutoSkip(const FString& BeatName);

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
