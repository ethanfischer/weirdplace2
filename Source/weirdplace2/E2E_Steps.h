#pragma once

// Reusable step functions for E2E tests. Each function enqueues latent
// commands for one logical step. Included by test .cpp files that need
// to compose steps into full or partial test runs.

#include "E2E_LatentCommands.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// Helper: open the map, wait for spawn.
#define E2E_TEST_PREAMBLE(Label) \
	UE_LOG(LogTemp, Warning, TEXT("=== E2E TEST START === " Label " %s"), *FDateTime::Now().ToString()); \
	AutomationOpenMap(TEXT("/Game/FirstPerson/Maps/FirstPersonMap")); \
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForPlayerReady(this));

namespace E2ESteps
{
	void SenecaIntro(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_01_AtSeneca")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(T, EPlayerActivityState::InDialogue));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_02_SenecaDialogueStarted")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_04_IntroDialogueDone")));
	}

	void CollectMovies(FAutomationTestBase* T)
	{
		const TCHAR* MovieLabels[] = {
			TEXT("BP_MovieBox120"),
			TEXT("BP_MovieBox121"),
			TEXT("BP_MovieBox122"),
		};
		for (int32 i = 0; i < 3; ++i)
		{
			// Stand in front of each box along its own forward vector and aim
			// at a trace-verified surface point — derived bounds centers hit
			// neighboring boxes at oblique angles.
			ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportFacingShelfBoxAndAim(T, MovieLabels[i]));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(T, EPlayerActivityState::Interacting));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_RotateAndCollectMovie(T));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(T, i + 1));
		}
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_05_ThreeMoviesCollected")));
	}

	void GiveMoviesToSeneca(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));

		// Give-mode: interacting with Seneca pops the inventory; select the movie
		// slot and press E to hand it over. Slots are persistent (OoT-style): movie
		// i stays at slot i even after earlier movies are given.
		for (int32 i = 0; i < 3; ++i)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // opens give inventory
			ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.6f));                 // wait for open animation
			if (i == 0)
				ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_06_InventoryOpenMovie1")));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, i));  // select movie slot
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // E confirms the give
			ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(FString::Printf(TEXT("E2E_%02d_GaveMovie%d"), 7 + i, i + 1)));
		}
	}

	void GetMoneyFromRick(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("RickApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtRick(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_10_TalkingToRick")));
		// Advance until the money mesh notification appears
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilItemNotification(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_11_GotMoney")));
		// Advance one more line — mesh should disappear
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_11b_MoneyMeshGone")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}

	void GiveMoneyAskForBlank(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // opens give inventory
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.6f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_12_InventoryWithMoney")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));  // Money at slot 0
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // E gives the money
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13_AskedForBlank")));
	}

	void CollectBlankTape(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearBlankTape(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtBlankTape(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(T, EPlayerActivityState::Interacting));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_RotateAndCollectMovie(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(T, FName("BlankVHS")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14_BlankTapeCollected")));
	}

	void GiveBlankTapeGetKey(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // opens give inventory
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.6f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));  // BlankVHS at slot 0 (Money is gone)
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // E gives the blank tape
		// Advance through the merged burn-flavor + key-handoff lines until the
		// [Give key] cue fires the key item notification.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilItemNotification(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_15_GotKey")));
		// Dismiss key, finish dialogue.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_15b_KeyMeshGone")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}

	void UseKeyOnDoor(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("OutsideBathroom")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorComponentByName(T, TEXT("BP_OutsideBathroomDoor"), TEXT("KeyLockSocket")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // door pops the inventory (give mode)
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.6f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_15_InventoryWithKey")));
		// Key lands in slot 0 now that the combined-tape movie stack is gone —
		// the post-key inventory only contains the Key.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));   // E gives the key -> break sequence

		// The key-break sequence (insert → turn → break → fall) no longer
		// auto-adds the broken key to inventory; it drops a collectable
		// AInspectablePickup on the ground. Wait for that pickup to spawn.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForInspectablePickupSpawned(T, 15.0));
		// The full Key is consumed by the break, and the broken half is NOT
		// in inventory yet — it has to be picked up off the ground.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertNotHasItem(T, FName("Key")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertNotHasItem(T, FName("BrokenKey")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_16_BrokenKeyDropped")));

		// Walk up, inspect (pull to camera), and collect the broken key the
		// same way any other inspectable item is collected.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearInspectablePickupAndAim(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(T, EPlayerActivityState::Interacting));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_16b_InspectingBrokenKey")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CollectInspectedPickup(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForItemAdded(T, FName("BrokenKey"), 5.0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_16c_BrokenKeyCollected")));
	}

	void WatchTornadoWarningTV(FAutomationTestBase* T)
	{
		// The key-break sequence set KeyBroke. Re-entering the store flips both
		// store TVs to the tornado warning; the player must dwell-gaze a warning
		// TV for StorySubsystem's GazeRequiredSeconds (2s) to register
		// SeenTornadoWarning — which reveals the roadside payphone and gates the
		// UseTelephone step below.
		//
		// Stand right in front of BP_TV: from the MovieShelf spot itself the
		// store's own movie-cover shelving occludes the TV (the gaze
		// line-of-sight trace is blocked by shelf VHS covers), so the dwell
		// never accrues. Teleport to MovieShelf FIRST so the subsequent
		// "near TV" placement — which lands the player on the TV side facing
		// their current position — ends up on the shelf side of the TV, which
		// has a clear sightline. (Approaching from the bathroom side, where the
		// player is after collecting the broken key, lands on the occluded side.)
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerStoreEntry(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("MovieShelf")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(T, TEXT("BP_TV"), 250.f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f)); // let the capsule settle before baking the look rotation
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BP_TV")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_16d_WatchingTornadoTV")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForStoryFlag(T, FName("SeenTornadoWarning"), true, 8.0));
	}

	void FastForwardSenecaSmoking(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_FastForwardSenecaSmoking(T));
	}

	void SenecaSmokingDialogue(FAutomationTestBase* T)
	{
		// The broken-key beat hides Seneca and schedules her to reappear at the
		// smoking spot after a 30s delay. Skip that wait so the test isn't gated
		// on real time (FTD_WaitForSenecaAppearedAtSmoking only allows a few
		// seconds) — same fast-forward the SenecaSmokingAnim diagnostic uses.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_FastForwardSenecaSmoking(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaAppearedAtSmoking(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_17_SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}
	
	void UseTelephone(FAutomationTestBase* T)
	{
		// SeenTornadoWarning (set by WatchTornadoWarningTV) revealed the roadside
		// payphone. Walk up, pick up the receiver — which records UsedPayPhone and
		// unlocks the bathroom keypad — then hang up via Exit Interaction to release
		// the player. There is no "Telephone" waypoint; the phone lives far off the
		// store lot, so teleport near the actor itself.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(T, TEXT("BP_TelephoneScene"), 300.f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BP_TelephoneScene")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_17a_Telephone")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForStoryFlag(T, FName("UsedPayPhone"), true, 5.0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulatePutBack(T));
	}

	void OpenBathroomDoor(FAutomationTestBase* T)
	{
		// Employee bathroom now opens via the phone-code keypad (Seneca no longer
		// unlocks it). The lock accepts almost any 4-digit entry, but it must contain
		// an 8 and not be a blocked "obvious" code — "4289" qualifies. Enter it by
		// selecting each digit cell (digit N lives at cell N-1) and pressing Interact.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("EmployeeBathroom")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BathroomDoor")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForKeypadOpen(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_19a_KeypadOpen")));
		// Enter the first two digits to capture the fill row (now above the grid), then
		// finish the code "4289" to open the door.
		ADD_LATENT_AUTOMATION_COMMAND(FTD_EnterKeypadCode(T, TEXT("42")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_19b_KeypadPartial")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_EnterKeypadCode(T, TEXT("89")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForDoorOpen(T, TEXT("BathroomDoor")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_19_BathroomDoorOpen")));
	}

	void EnterStall(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("ApproachStall")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LerpTo(T, TEXT("Teleporter"), 2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_20_AtStall")));
	}

	void ExitBathroom(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LerpTo(T, TEXT("OasisDoor"), 2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BathroomDoor2")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForDoorOpen(T, TEXT("BathroomDoor2")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtWaypoint(T, TEXT("OasisCenter")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_21_Done")));
	}

} // namespace E2ESteps

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
