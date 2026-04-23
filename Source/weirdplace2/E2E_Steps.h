#pragma once

// Reusable step functions for E2E tests. Each function enqueues latent
// commands for one logical step. Included by test .cpp files that need
// to compose steps into full or partial test runs.

#include "E2E_LatentCommands.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

// Helper: open the map, wait for spawn, suppress the known widget warning.
#define E2E_TEST_PREAMBLE(Label) \
	UE_LOG(LogTemp, Warning, TEXT("=== E2E TEST START === " Label " %s"), *FDateTime::Now().ToString()); \
	AddExpectedError(TEXT("InteractionText widget not found"), EAutomationExpectedErrorFlags::Contains, 0); \
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
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::WaitingForItemInteractionInDialogue));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_03_BasketBeat")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("ShoppingBasket")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_04_IntroDialogueDone")));
	}

	void CollectMovies(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("MovieShelf")));

		const TCHAR* MovieLabels[] = {
			TEXT("BP_MovieBox120"),
			TEXT("BP_MovieBox121"),
			TEXT("BP_MovieBox122"),
		};
		for (int32 i = 0; i < 3; ++i)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, MovieLabels[i]));
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

		for (int32 i = 0; i < 3; ++i)
		{
			ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(T));
			if (i == 0)
				ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_06_InventoryOpenMovie1")));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(T));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
			ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
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

	void GiveMoneyGetKey(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_12_InventoryWithMoney")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		// Advance until the movie stack notification appears
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilItemNotification(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13_GotMovies")));
		// Dismiss movie stack, continue dialogue
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13b_MoviesGone")));
		// Advance until the key mesh notification appears
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilItemNotification(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14_GotKey")));
		// Dismiss key, continue dialogue
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14b_KeyMeshGone")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}

	void UseKeyOnDoor(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("OutsideBathroom")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_15_InventoryWithKey")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 3));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorComponentByName(T, TEXT("BP_OutsideBathroomDoor"), TEXT("KeyLockSocket")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForItemAdded(T, FName("BrokenKey"), 8.0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertNotHasItem(T, FName("Key")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_16_KeyBroken")));
	}

	void FastForwardSenecaSmoking(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_FastForwardSenecaSmoking(T));
	}

	void SenecaSmokingDialogue(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaAppearedAtSmoking(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_17_SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}

	void SenecaHallwayDialogue(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaHallway")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaState(T, ESenecaState::AtEmployeeBathroom));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaState(T, ESenecaState::Done));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_18_SenecaDone")));
	}

	void OpenBathroomDoor(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("EmployeeBathroom")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BathroomDoor")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
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
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorComponentByName(T, TEXT("BathroomDoor2"), TEXT("DoorHandle")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForDoorOpen(T, TEXT("BathroomDoor2")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtWaypoint(T, TEXT("OasisCenter")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_21_Done")));
	}

} // namespace E2ESteps

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
