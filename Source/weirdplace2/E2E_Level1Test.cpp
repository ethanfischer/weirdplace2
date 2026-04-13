#include "E2E_Steps.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// Helper: open the map, wait for spawn, suppress the known widget warning.
#define E2E_TEST_PREAMBLE(Label) \
	UE_LOG(LogTemp, Warning, TEXT("=== E2E TEST START === " Label " %s"), *FDateTime::Now().ToString()); \
	AddExpectedError(TEXT("InteractionText widget not found"), EAutomationExpectedErrorFlags::Contains, 0); \
	AutomationOpenMap(TEXT("/Game/FirstPerson/Maps/FirstPersonMap")); \
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForPlayerReady(this));

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
// Isolated step tests — one per step so each can run independently
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_SenecaIntro, "Weirdplace2.E2E.Steps.SenecaIntro",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_SenecaIntro::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SenecaIntro")
	E2ESteps::SenecaIntro(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_CollectMovies, "Weirdplace2.E2E.Steps.CollectMovies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_CollectMovies::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("CollectMovies")
	E2ESteps::CollectMovies(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_GiveMoviesToSeneca, "Weirdplace2.E2E.Steps.GiveMoviesToSeneca",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_GiveMoviesToSeneca::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("GiveMoviesToSeneca")
	// Requires movies in inventory — run the prerequisite steps
	E2ESteps::SenecaIntro(this);
	E2ESteps::CollectMovies(this);
	E2ESteps::GiveMoviesToSeneca(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_GetMoneyFromRick, "Weirdplace2.E2E.Steps.GetMoneyFromRick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_GetMoneyFromRick::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("GetMoneyFromRick")
	E2ESteps::GetMoneyFromRick(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_GiveMoneyGetKey, "Weirdplace2.E2E.Steps.GiveMoneyGetKey",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_GiveMoneyGetKey::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("GiveMoneyGetKey")
	// Requires money in inventory — run prerequisite steps
	E2ESteps::SenecaIntro(this);
	E2ESteps::CollectMovies(this);
	E2ESteps::GiveMoviesToSeneca(this);
	E2ESteps::GetMoneyFromRick(this);
	E2ESteps::GiveMoneyGetKey(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_UseKeyOnDoor, "Weirdplace2.E2E.Steps.UseKeyOnDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_UseKeyOnDoor::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("UseKeyOnDoor")
	// Requires key in inventory — run prerequisite steps
	E2ESteps::SenecaIntro(this);
	E2ESteps::CollectMovies(this);
	E2ESteps::GiveMoviesToSeneca(this);
	E2ESteps::GetMoneyFromRick(this);
	E2ESteps::GiveMoneyGetKey(this);
	E2ESteps::UseKeyOnDoor(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_SenecaSmokingDialogue, "Weirdplace2.E2E.Steps.SenecaSmokingDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_SenecaSmokingDialogue::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SenecaSmokingDialogue")
	E2ESteps::FastForwardSenecaSmoking(this);
	E2ESteps::SenecaSmokingDialogue(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_SenecaHallwayDialogue, "Weirdplace2.E2E.Steps.SenecaHallwayDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_SenecaHallwayDialogue::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SenecaHallwayDialogue")
	// Seneca must reach AtEmployeeBathroom state — run prerequisite steps
	E2ESteps::FastForwardSenecaSmoking(this);
	E2ESteps::SenecaSmokingDialogue(this);
	E2ESteps::SenecaHallwayDialogue(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_OpenBathroomDoor, "Weirdplace2.E2E.Steps.OpenBathroomDoor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_OpenBathroomDoor::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("OpenBathroomDoor")
	E2ESteps::OpenBathroomDoor(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_EnterStall, "Weirdplace2.E2E.Steps.EnterStall",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_EnterStall::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("EnterStall")
	E2ESteps::EnterStall(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FE2E_ExitBathroom, "Weirdplace2.E2E.Steps.ExitBathroom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FE2E_ExitBathroom::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("ExitBathroom")
	E2ESteps::EnterStall(this);
	E2ESteps::ExitBathroom(this);
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// BathroomDoorTraceRepro — standalone diagnostic, not a step
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

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
