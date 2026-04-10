#include "E2E_LatentCommands.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// The happy-path test itself. All FTD_* latent command classes live in
// E2E_LatentCommands.h — keep this file focused on test body/sequencing.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_HappyPath,
	"Weirdplace2.E2E.Level1.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_HappyPath::RunTest(const FString& Parameters)
{
	// Distinctive marker so log grep can scope to the most recent run.
	// grep for "=== E2E TEST START ===" and read lines after the last match.
	UE_LOG(LogTemp, Warning, TEXT("=== E2E TEST START === %s"), *FDateTime::Now().ToString());

	// Suppress pre-existing errors unrelated to the test.
	AddExpectedError(TEXT("JPEG Decompress Error"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("TryDecompressData failed"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("InteractionText widget not found"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Unable to get texture source data"), EAutomationExpectedErrorFlags::Contains, 0);

	AutomationOpenMap(TEXT("/Game/FirstPerson/Maps/FirstPersonMap"));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForPlayerReady(this));

	// --- Step 1: Initial dialogue with Seneca (basket beat) ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_01_AtSeneca")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // interact with Seneca
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InMultiSpeakerDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_02_SenecaDialogueStarted")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::WaitingForItemInteractionInDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_03_BasketBeat")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("ShoppingBasket")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // pick up basket
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_04_IntroDialogueDone")));

	// --- Step 2: Collect 3 specific movies from the MovieShelf waypoint ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("MovieShelf")));

	const TCHAR* MovieLabels[] = {
		TEXT("BP_MovieBox120"),
		TEXT("BP_MovieBox121"),
		TEXT("BP_MovieBox122"),
	};
	for (int32 i = 0; i < 3; ++i)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, MovieLabels[i]));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // E (Enhanced Input) -> enter inspection
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::Interacting));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_RotateAndCollectMovie(this));   // rotate + legacy-E collect
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, i + 1));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_05_ThreeMoviesCollected")));

	// --- Step 3: Give each movie to Seneca ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));

	for (int32 i = 0; i < 3; ++i)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
		if (i == 0) ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_06_InventoryOpenMovie1")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, 0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // give movie to Seneca
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(FString::Printf(TEXT("E2E_%02d_GaveMovie%d"), 7 + i, i + 1)));
	}

	// --- Step 4: Get money from Rick ---
	// TODO: replace with FTD_TeleportTo("RickApproach") once the waypoint is placed.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearRick(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtRick(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // talk to Rick
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_10_TalkingToRick")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_11_GotMoney")));

	// --- Step 5: Give money to Seneca + key beat ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_12_InventoryWithMoney")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // give money
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::WaitingForItemInteractionInDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13_KeyBeat")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtKeyActor(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));  // pick up key
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14_GotKey")));

	// --- Final assertion ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(this, FName("Key")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
