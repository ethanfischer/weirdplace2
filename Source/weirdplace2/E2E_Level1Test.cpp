#include "E2E_Steps.h" // force rebuild

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// =======================================================================
// Full happy-path test
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_HappyPath,
	"Weirdplace2.E2E.Level1.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_HappyPath::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("HappyPath")

	E2ESteps::SenecaIntro(this);
	E2ESteps::CollectMovies(this);
	E2ESteps::GiveMoviesToSeneca(this);
	E2ESteps::GetMoneyFromRick(this);
	E2ESteps::GiveMoneyAskForBlank(this);
	E2ESteps::CollectBlankTape(this);
	E2ESteps::GiveBlankTapeGetKey(this);
	E2ESteps::UseKeyOnDoor(this);
	E2ESteps::FastForwardSenecaSmoking(this);
	E2ESteps::SenecaSmokingDialogue(this);
	E2ESteps::SenecaHallwayDialogue(this);
	E2ESteps::OpenBathroomDoor(this);
	E2ESteps::EnterStall(this);
	E2ESteps::ExitBathroom(this);

	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(this, FName("BrokenKey")));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// BathroomDoorTraceRepro — standalone diagnostic
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_BathroomDoorTraceRepro,
	"Weirdplace2.E2E.Level1.BathroomDoorTraceRepro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_BathroomDoorTraceRepro::RunTest(const FString& Parameters)
{
	UE_LOG(LogTemp, Warning, TEXT("=== E2E TEST START === BathroomDoorTraceRepro %s"), *FDateTime::Now().ToString());

	AddExpectedError(TEXT("JPEG Decompress Error"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("TryDecompressData failed"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("InteractionText widget not found"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Unable to get texture source data"), EAutomationExpectedErrorFlags::Contains, 0);

	AutomationOpenMap(TEXT("/Game/FirstPerson/Maps/FirstPersonMap"));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForPlayerReady(this));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("BathroomDoor"), 250.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BathroomDoor")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Repro_BathroomDoorAim")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// DialogueCooldown — verify the 2-second post-dialogue interaction
// cooldown prevents re-triggering dialogue when spamming E, and that
// interaction works again after the cooldown expires.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_DialogueCooldown,
	"Weirdplace2.E2E.Level1.DialogueCooldown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_DialogueCooldown::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("DialogueCooldown")

	// Approach Hudson and start his idle dialogue
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearHudson(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtHudson(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InSimpleDialogue));

	// Advance dialogue to completion
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));

	// Immediately try to interact again — cooldown should block it
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertActivityState(this, EPlayerActivityState::FreeRoaming));

	// Wait for the 2-second cooldown to expire
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(2.5f));

	// Now interaction should work again
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtHudson(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InSimpleDialogue));

	// Cleanup — finish the dialogue
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// SensitivityScaling — diagnostic test for the gamepad look-sensitivity
// slider. Drives the LookAction via mouse-axis injection at two slider
// values and at idle, comparing actual ControlRotation deltas.
//
// What each phase tells us:
//   1. High slider (1.0) + injection → camera should rotate noticeably.
//      If delta is ~0, mouse injection is NOT reaching HandleLookInput
//      (broken IMC binding, IsLookInputIgnored, etc.).
//   2. Low slider (0.1) + injection → camera should be nearly frozen.
//      If delta is large, the slider isn't actually crushing the input.
//   3. Idle (no injection) → camera must NOT drift. Any non-trivial delta
//      here is the smoking gun: something OUTSIDE HandleLookInput is
//      rotating the camera (Blueprint, default Pawn binding, gamepad
//      drift in headless test, etc.).
// =======================================================================

namespace
{
	static float CapturedYaw_High = 0.f;
	static float CapturedYaw_Low = 0.f;
	static float CapturedYaw_Idle = 0.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_SensitivityScaling,
	"Weirdplace2.E2E.Level1.SensitivityScaling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_SensitivityScaling::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SensitivityScaling")

	// Reset captures so re-runs in the same editor session aren't comparing
	// against a previous run's leftover yaw.
	CapturedYaw_High = 0.f;
	CapturedYaw_Low = 0.f;
	CapturedYaw_Idle = 0.f;

	// Settle a moment after spawn so any startup transient input has flushed.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));

	// --- Phase 1: high sensitivity, expect noticeable rotation ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetMouseLookSensitivity(this, 1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureYaw(this, &CapturedYaw_High));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InjectMouseXForDuration(this, 50.0f, 1.0f));
	// Generous range: just verify mouse injection produced meaningful rotation.
	// Tight upper bound would be brittle under different IMC modifier configs.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertYawDelta(this, TEXT("HighSlider1.0_Mouse"),
		&CapturedYaw_High, /*Min=*/1.0f, /*Max=*/1000.0f));

	// --- Phase 2: low sensitivity, expect ~10x smaller rotation ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetMouseLookSensitivity(this, 0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureYaw(this, &CapturedYaw_Low));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InjectMouseXForDuration(this, 50.0f, 1.0f));
	// Mouse curve is linear (V * 1.0): with identical injection to phase 1, the
	// actual delta should be ~10x smaller. Max=150 catches "slider not applied"
	// (which would reproduce phase-1 magnitudes well above this bound).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertYawDelta(this, TEXT("LowSlider0.1_Mouse"),
		&CapturedYaw_Low, /*Min=*/0.0f, /*Max=*/150.0f));

	// --- Phase 3: idle drift check (the smoking-gun test) ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureYaw(this, &CapturedYaw_Idle));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(2.0f));
	// No input injected; camera must not move. Any drift here means a
	// non-HandleLookInput rotation source exists.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertYawDelta(this, TEXT("Idle"),
		&CapturedYaw_Idle, /*Min=*/0.0f, /*Max=*/0.1f));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// PauseMenu — verify the pause menu wraps the settings page: pressing the
// settings key opens the Pause page, navigating to "Settings" + confirm
// swaps to the Settings page in place, "Back" returns, and pressing the
// settings key again closes the menu entirely.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_PauseMenu,
	"Weirdplace2.E2E.Level1.PauseMenu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_PauseMenu::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("PauseMenu")

	// Open the menu — Pause page should appear.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateSettingsPress(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::Interacting));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertMenuPage(this, EMenuPage::Pause));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenu_01_PauseOpen")));

	// Navigate down once (Resume → Settings) and confirm to swap to Settings page.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateNavAction(this, ENavInputAction::NextOption));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertMenuPage(this, EMenuPage::Settings));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenu_02_SettingsAfterSwap")));

	// Settings page: Gamepad → Mouse → Back, then confirm Back.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateNavAction(this, ENavInputAction::NextOption));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateNavAction(this, ENavInputAction::NextOption));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertMenuPage(this, EMenuPage::Pause));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenu_03_BackToPause")));

	// Press the settings key again — menu should close entirely.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateSettingsPress(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenu_04_Closed")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// PauseMenuLight — verify the player's RectLight (the same one the
// inventory uses) is enabled while the pause menu is open and disabled
// after it closes.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_PauseMenuLight,
	"Weirdplace2.E2E.Level1.PauseMenuLight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_PauseMenuLight::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("PauseMenuLight")

	// Baseline: light off before any UI is opened.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryFlashlight(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenuLight_01_Before")));

	// Open the menu and wait past the open animation so the light has flipped on.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateSettingsPress(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::Interacting));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryFlashlight(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenuLight_02_MenuOpenLightOn")));

	// Close the menu — light should disable immediately on close.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateSettingsPress(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryFlashlight(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenuLight_03_AfterClose")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// InventoryThumbnails — verify that inventory thumbnails render at a
// consistent brightness regardless of the surrounding scene's lighting.
// The M_ItemThumbnail material divides emissive by EyeAdaptation so the
// tonemapper's auto-exposure multiplication cancels out. Without that fix,
// thumbnails blow out white in dim scenes.
//
// The test injects Money + Key into the inventory, opens the inventory in
// the bright outdoor parking lot (PlayerStart) for a baseline screenshot,
// then teleports somewhere dim and takes a second screenshot. Both should
// show the thumbnails at similar brightness — visually verifiable by
// comparing the two PNGs.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_InventoryThumbnails,
	"Weirdplace2.E2E.Level1.InventoryThumbnails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_InventoryThumbnails::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("InventoryThumbnails")

	// Bypass Seneca-intro gate that normally blocks inventory open.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_UnlockInventory(this));

	// Inject Money + Key directly so we can focus on rendering, not gameplay.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("Money"),
		TEXT("/Game/Import/cash/cash.cash"), FVector(1.0f)));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("Key"),
		TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 2));

	// Bright outdoor — open inventory + screenshot.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Inv_01_Bright")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));

	// Dim interior — same inventory, screenshot again. SenecaApproach is
	// inside the store under store lighting which is much dimmer than outside.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Inv_02_Dim")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// HeldItemRotationTour — grant the player every UItemDefinition under
// /Game/Inventory, then cycle each one as the active (held) item with a
// delay between cycles so the held mesh is visible in front of the camera.
// Use the screenshots to eyeball each item's HeldRotation on its data asset.
//
// Add a new DA_*.uasset under /Game/Inventory and rerun — it'll show up
// automatically (just bump NumExpected if you want to assert the count).
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_HeldItemRotationTour,
	"Weirdplace2.E2E.Level1.HeldItemRotationTour",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_HeldItemRotationTour::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("HeldItemRotationTour")

	ADD_LATENT_AUTOMATION_COMMAND(FTD_UnlockInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddAllItemDefsFromFolder(this, TEXT("/Game/Inventory"), /*ExpectedMin*/ 1));

	// Lit interior so the small held meshes are visible.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));

	// Currently 4 defs in /Game/Inventory; bump if you add more. Each loop
	// iteration: open inventory, select slot, close, wait, screenshot the
	// held mesh in front of the camera.
	const int32 NumSlots = 4;
	for (int32 i = 0; i < NumSlots; ++i)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, i));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(FString::Printf(TEXT("E2E_HeldTour_%02d"), i)));
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
