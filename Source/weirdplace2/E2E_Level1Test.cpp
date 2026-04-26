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
	E2ESteps::GiveMoneyGetKey(this);
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
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetGamepadLookSensitivity(this, 1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureYaw(this, &CapturedYaw_High));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InjectMouseXForDuration(this, 50.0f, 1.0f));
	// Generous range: just verify mouse injection produced meaningful rotation.
	// Tight upper bound would be brittle under different IMC modifier configs.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertYawDelta(this, TEXT("HighSlider1.0"),
		&CapturedYaw_High, /*Min=*/1.0f, /*Max=*/1000.0f));

	// --- Phase 2: low sensitivity, expect near-zero rotation ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetGamepadLookSensitivity(this, 0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.25f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureYaw(this, &CapturedYaw_Low));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InjectMouseXForDuration(this, 50.0f, 1.0f));
	// Quadratic curve: scale is 0.005 here (V*V*0.5). With identical injection
	// to phase 1, the actual delta should be ~100x smaller. If this fails, the
	// slider math is not being applied through the rotation overrides.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertYawDelta(this, TEXT("LowSlider0.1"),
		&CapturedYaw_Low, /*Min=*/0.0f, /*Max=*/10.0f));

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

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
