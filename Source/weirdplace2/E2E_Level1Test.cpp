#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "TestDriverSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

// =======================================================================
// Helpers for getting at the PIE world and the TestDriver subsystem.
// =======================================================================

namespace
{
	UWorld* GetPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if ((Ctx.WorldType == EWorldType::PIE || Ctx.WorldType == EWorldType::Game) && Ctx.World())
			{
				return Ctx.World();
			}
		}
		return nullptr;
	}

	UTestDriverSubsystem* GetDriver()
	{
		UWorld* World = GetPIEWorld();
		return World ? World->GetSubsystem<UTestDriverSubsystem>() : nullptr;
	}
}

// =======================================================================
// Latent command: wait for PIE world + player to be initialized.
// =======================================================================

class FTD_WaitForPlayerReady : public IAutomationLatentCommand
{
public:
	FTD_WaitForPlayerReady(FAutomationTestBase* InTest, double InTimeoutSeconds = 15.0)
		: Test(InTest), StartTime(FPlatformTime::Seconds()), Timeout(InTimeoutSeconds) {}

	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (Driver && Driver->IsPlayerReady())
		{
			return true;
		}

		if (FPlatformTime::Seconds() - StartTime > Timeout)
		{
			Test->AddError(TEXT("FTD_WaitForPlayerReady: player never became ready (timeout)"));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	double StartTime;
	double Timeout;
};

// =======================================================================
// Simple "fire once then done" latent commands (parameterized)
// =======================================================================

class FTD_TeleportTo : public IAutomationLatentCommand
{
public:
	FTD_TeleportTo(FAutomationTestBase* InTest, FName InTag) : Test(InTest), Tag(InTag) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver)
		{
			Test->AddError(TEXT("FTD_TeleportTo: no TestDriverSubsystem"));
			return true;
		}
		if (!Driver->TeleportPlayerToWaypoint(Tag))
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportTo: waypoint '%s' not found"), *Tag.ToString()));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	FName Tag;
};

class FTD_LookAtActorByLabel : public IAutomationLatentCommand
{
public:
	FTD_LookAtActorByLabel(FAutomationTestBase* InTest, FString InLabel) : Test(InTest), Label(MoveTemp(InLabel)) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtActorByLabel: no driver")); return true; }
		if (!Driver->LookAtActorByLabel(Label))
		{
			Test->AddError(FString::Printf(TEXT("FTD_LookAtActorByLabel: no actor '%s'"), *Label));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	FString Label;
};

class FTD_PressInteract : public IAutomationLatentCommand
{
public:
	FTD_PressInteract(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_PressInteract: no driver")); return true; }
		Driver->PressInteract();
		return true;
	}
private:
	FAutomationTestBase* Test;
};

class FTD_InteractWithSeneca : public IAutomationLatentCommand
{
public:
	FTD_InteractWithSeneca(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_InteractWithSeneca: no driver")); return true; }
		if (!Driver->InteractWithSeneca())
		{
			Test->AddError(TEXT("FTD_InteractWithSeneca: failed"));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
};

class FTD_InteractWithRick : public IAutomationLatentCommand
{
public:
	FTD_InteractWithRick(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_InteractWithRick: no driver")); return true; }
		if (!Driver->InteractWithRick())
		{
			Test->AddError(TEXT("FTD_InteractWithRick: failed"));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
};

class FTD_InteractWithKeyActor : public IAutomationLatentCommand
{
public:
	FTD_InteractWithKeyActor(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_InteractWithKeyActor: no driver")); return true; }
		if (!Driver->InteractWithKeyActor())
		{
			Test->AddError(TEXT("FTD_InteractWithKeyActor: failed"));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
};

class FTD_InteractWithActorByLabel : public IAutomationLatentCommand
{
public:
	FTD_InteractWithActorByLabel(FAutomationTestBase* InTest, FString InLabel)
		: Test(InTest), Label(MoveTemp(InLabel)) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_InteractWithActorByLabel: no driver")); return true; }
		if (!Driver->InteractWithActorByLabel(Label))
		{
			Test->AddError(FString::Printf(TEXT("FTD_InteractWithActorByLabel: failed for '%s'"), *Label));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	FString Label;
};

// =======================================================================
// Dialogue-advance loop. Keeps pressing interact while the player is in
// a Simple or MultiSpeaker dialogue state, until either:
//   (a) the activity state becomes `Target` (usually FreeRoaming or
//       WaitingForItemInteractionInDialogue for a beat), or
//   (b) the timeout fires.
// Any other state during the loop is treated as an error.
// =======================================================================

class FTD_AdvanceDialogueUntilState : public IAutomationLatentCommand
{
public:
	FTD_AdvanceDialogueUntilState(FAutomationTestBase* InTest, EPlayerActivityState InTarget, double InTimeoutSeconds = 10.0)
		: Test(InTest), Target(InTarget), StartTime(FPlatformTime::Seconds()), Timeout(InTimeoutSeconds), LastLoggedState((EPlayerActivityState)-1) {}

	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		const EPlayerActivityState State = Driver->GetActivityState();
		if (State != LastLoggedState)
		{
			UE_LOG(LogTemp, Log, TEXT("FTD_AdvanceDialogueUntilState: state=%d target=%d"), (int32)State, (int32)Target);
			LastLoggedState = State;
		}
		if (State == Target)
		{
			return true;
		}

		const bool bInDialogue =
			State == EPlayerActivityState::InSimpleDialogue ||
			State == EPlayerActivityState::InMultiSpeakerDialogue;

		if (!bInDialogue)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AdvanceDialogueUntilState: unexpected activity state %d (expected dialogue or target %d)"),
				(int32)State, (int32)Target));
			return true;
		}

		if (FPlatformTime::Seconds() - StartTime > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AdvanceDialogueUntilState: timed out in state %d"), (int32)State));
			return true;
		}

		Driver->PressInteract(); // advances one line (dispatches on state)
		return false;
	}
private:
	FAutomationTestBase* Test;
	EPlayerActivityState Target;
	double StartTime;
	double Timeout;
	EPlayerActivityState LastLoggedState;
};

// =======================================================================
// Wait until activity state matches expected (with timeout).
// =======================================================================

class FTD_WaitForActivityState : public IAutomationLatentCommand
{
public:
	FTD_WaitForActivityState(FAutomationTestBase* InTest, EPlayerActivityState InExpected, double InTimeoutSeconds = 5.0)
		: Test(InTest), Expected(InExpected), StartTime(FPlatformTime::Seconds()), Timeout(InTimeoutSeconds) {}

	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (Driver->GetActivityState() == Expected)
		{
			return true;
		}
		if (FPlatformTime::Seconds() - StartTime > Timeout)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_WaitForActivityState: timed out waiting for state %d (current %d)"),
				(int32)Expected, (int32)Driver->GetActivityState()));
			return true;
		}
		return false;
	}
private:
	FAutomationTestBase* Test;
	EPlayerActivityState Expected;
	double StartTime;
	double Timeout;
};

// =======================================================================
// Inventory UI helpers.
// =======================================================================

class FTD_OpenInventory : public IAutomationLatentCommand
{
public:
	FTD_OpenInventory(FAutomationTestBase* InTest) : Test(InTest), bIssued(false) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (!bIssued)
		{
			Driver->OpenInventory();
			bIssued = true;
		}

		// Wait until the open animation finishes so ConfirmSelection will work.
		return Driver->IsInventoryFullyOpen();
	}
private:
	FAutomationTestBase* Test;
	bool bIssued;
};

class FTD_CloseInventory : public IAutomationLatentCommand
{
public:
	FTD_CloseInventory(FAutomationTestBase* InTest) : Test(InTest), bIssued(false) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (!bIssued)
		{
			Driver->CloseInventory();
			bIssued = true;
		}

		// Wait until the close animation finishes so CanInteract is restored
		// before the next PressInteract runs.
		return Driver->IsInventoryFullyClosed();
	}
private:
	FAutomationTestBase* Test;
	bool bIssued;
};

class FTD_SelectInventoryItemByIndex : public IAutomationLatentCommand
{
public:
	FTD_SelectInventoryItemByIndex(FAutomationTestBase* InTest, int32 InIndex) : Test(InTest), Index(InIndex) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (!Driver->SelectInventoryItemByIndex(Index))
		{
			Test->AddError(FString::Printf(TEXT("FTD_SelectInventoryItemByIndex: failed to select slot %d"), Index));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	int32 Index;
};

// =======================================================================
// Movie collection (enter inspection + collect atomically).
// =======================================================================

class FTD_CollectNextMovie : public IAutomationLatentCommand
{
public:
	FTD_CollectNextMovie(FAutomationTestBase* InTest) : Test(InTest) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (!Driver->CollectNextMovie())
		{
			Test->AddError(TEXT("FTD_CollectNextMovie: failed to collect"));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
};

// =======================================================================
// Screenshots.
// =======================================================================

class FTD_TakeScreenshot : public IAutomationLatentCommand
{
public:
	FTD_TakeScreenshot(const FString& InName) : Name(InName), bRequested(false) {}
	virtual bool Update() override
	{
		if (!bRequested)
		{
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			bRequested = true;
			return false;
		}
		return !FScreenshotRequest::IsScreenshotRequested();
	}
private:
	FString Name;
	bool bRequested;
};

// =======================================================================
// Assertions.
// =======================================================================

class FTD_AssertInventoryCount : public IAutomationLatentCommand
{
public:
	FTD_AssertInventoryCount(FAutomationTestBase* InTest, int32 InExpected) : Test(InTest), Expected(InExpected) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		const int32 Actual = Driver->GetInventoryCount();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("InventoryCount: expected %d, got %d"), Expected, Actual));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	int32 Expected;
};

class FTD_AssertHasItem : public IAutomationLatentCommand
{
public:
	FTD_AssertHasItem(FAutomationTestBase* InTest, FName InItemId) : Test(InTest), ItemId(InItemId) {}
	virtual bool Update() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (!Driver->HasItem(ItemId))
		{
			Test->AddError(FString::Printf(TEXT("HasItem: expected '%s' in inventory"), *ItemId.ToString()));
		}
		return true;
	}
private:
	FAutomationTestBase* Test;
	FName ItemId;
};

// =======================================================================
// The happy-path test itself.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_HappyPath,
	"Weirdplace2.E2E.Level1.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_HappyPath::RunTest(const FString& Parameters)
{
	// Suppress pre-existing errors that are unrelated to the test but would
	// cause the automation framework to mark the run as failed.
	AddExpectedError(TEXT("JPEG Decompress Error"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("TryDecompressData failed"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("InteractionText widget not found"), EAutomationExpectedErrorFlags::Contains, 0);
	AddExpectedError(TEXT("Unable to get texture source data"), EAutomationExpectedErrorFlags::Contains, 0);

	// Load the first-person map and start PIE. These helpers are Epic's
	// standard pattern for editor-context automation tests.
	AutomationOpenMap(TEXT("/Game/FirstPerson/Maps/FirstPersonMap"));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForPlayerReady(this));

	// --- Step 1: Initial dialogue with Seneca, including the basket beat ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_01_AtSeneca")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InMultiSpeakerDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_02_SenecaDialogueStarted")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::WaitingForItemInteractionInDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_03_BasketBeat")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithActorByLabel(this, TEXT("ShoppingBasket")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_04_IntroDialogueDone")));

	// --- Step 2: Collect 3 movies ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CollectNextMovie(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 1));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CollectNextMovie(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 2));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CollectNextMovie(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 3));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_05_ThreeMoviesCollected")));

	// --- Step 3: Give each movie to Seneca ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaApproach")));

	// Movie 1
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_06_InventoryOpenMovie1")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectInventoryItemByIndex(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_07_GaveMovie1")));

	// Movie 2
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectInventoryItemByIndex(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_08_GaveMovie2")));

	// Movie 3 — after this one Seneca transitions to WaitingForMoney internally.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectInventoryItemByIndex(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_09_GaveMovie3_WaitingForMoney")));

	// --- Step 4: Get money from Rick ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithRick(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_10_TalkingToRick")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_11_GotMoney")));

	// --- Step 5: Give money to Seneca ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_12_InventoryWithMoney")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectInventoryItemByIndex(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventory(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::WaitingForItemInteractionInDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_13_KeyBeat")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InteractWithKeyActor(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilState(this, EPlayerActivityState::FreeRoaming));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_14_GotKey")));

	// --- Final assertion: player received the key ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(this, FName("Key")));

	// Stop PIE at the end of the test.
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
