#pragma once

// Reusable step functions for E2E tests. Each function enqueues latent
// commands for one logical step. Included by test .cpp files that need
// to compose steps into full or partial test runs.

#include "E2E_LatentCommands.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

namespace E2ESteps
{
	void Step1_SenecaIntro(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_01_AtSeneca")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(T, EPlayerActivityState::InMultiSpeakerDialogue));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_02_SenecaDialogueStarted")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::WaitingForItemInteractionInDialogue));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_03_BasketBeat")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("ShoppingBasket")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_04_IntroDialogueDone")));
	}

	void Step2_CollectMovies(FAutomationTestBase* T)
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

	void Step3_GiveMovies(FAutomationTestBase* T)
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

	void Step4_GetMoney(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("RickApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtRick(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_10_TalkingToRick")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_11_GotMoney")));
	}

	void Step5_GiveMoneyGetKey(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaApproach")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_12_InventoryWithMoney")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(T, 0));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::WaitingForItemInteractionInDialogue));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13_KeyBeat")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtKeyActor(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14_GotKey")));
	}

	void Step6_UseKey(FAutomationTestBase* T)
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

	void Step7_FastForwardSmoking(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_FastForwardSenecaSmoking(T));
	}

	void Step8_SenecaSmoking(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaAppearedAtSmoking(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_17_SenecaSmoking")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
	}

	void Step9_SenecaHallway(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("SenecaHallway")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaState(T, ESenecaState::AtEmployeeBathroom));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(T, EPlayerActivityState::FreeRoaming));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaState(T, ESenecaState::Done));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_18_SenecaDone")));
	}

	void Step10_OpenBathroomDoor(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("EmployeeBathroom")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(T, TEXT("BathroomDoor")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(T));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForDoorOpen(T, TEXT("BathroomDoor")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_19_BathroomDoorOpen")));
	}

	void Step11_EnterStall(FAutomationTestBase* T)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(T, TEXT("ApproachStall")));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_LerpTo(T, TEXT("Teleporter"), 2.0f));
		ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_20_AtStall")));
	}

	void Step12_ExitBathroom(FAutomationTestBase* T)
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
