// End-to-end gameplay tests for weirdplace2.
//
// These are NOT unit tests. Each test loads the real FirstPersonMap in
// Play-In-Editor (PIE), drives the real player character through the real
// gameplay systems, and asserts against observable game state. They are
// designed to mimic player behavior: move, interact with world objects,
// change inventory, transition activity state.
//
// Run from the command line:
//   "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
//     "C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" ^
//     -ExecCmds="Automation RunTests Weirdplace2.E2E; Quit" ^
//     -unattended -nopause -nosplash
//
// Or from the editor: Tools -> Session Frontend -> Automation -> filter
// "Weirdplace2.E2E".
//
// Tests only compile in editor builds with automation enabled (development
// editor target). They are stripped from shipping builds via the guards
// below.

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

#include "FirstPersonCharacter.h"
#include "Interactable.h"
#include "Inventory.h"
#include "MovieBox.h"
#include "MyCharacter.h"

// Flags shared by every E2E test. Defined as a macro (not a constexpr) so the
// exact expression is re-evaluated at each IMPLEMENT_SIMPLE_AUTOMATION_TEST
// call site — this keeps us compatible with both the namespace-wrapped enum
// and enum-class forms of EAutomationTestFlags across UE 5.x versions.
#define WEIRDPLACE2_E2E_TEST_FLAGS \
	(EAutomationTestFlags::EditorContext | \
	 EAutomationTestFlags::ClientContext | \
	 EAutomationTestFlags::ProductFilter)

namespace Weirdplace2E2E
{
	// Default E2E test map. Tests can override by passing parameters to
	// RunTest, but today everything exercises the main FirstPersonMap.
	static const FString DefaultTestMap = TEXT("/Game/FirstPerson/Maps/FirstPersonMap");

	// Returns the active PIE world, or nullptr if PIE has not started yet.
	static UWorld* GetPIEWorld()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && Context.World() != nullptr)
			{
				return Context.World();
			}
		}
		return nullptr;
	}

	static AMyCharacter* GetPlayerCharacter(UWorld* World)
	{
		if (!World)
		{
			return nullptr;
		}
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			return Cast<AMyCharacter>(PC->GetPawn());
		}
		return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Latent commands
// ---------------------------------------------------------------------------

// Polls until a PIE world exists AND has a possessed player pawn, or times
// out. Map load + game mode BeginPlay takes a variable number of ticks so we
// poll instead of hard-sleeping.
class FWeirdplace2WaitForPlayerPawn : public IAutomationLatentCommand
{
public:
	FWeirdplace2WaitForPlayerPawn(FAutomationTestBase* InTest, float InTimeoutSeconds)
		: Test(InTest)
		, TimeoutSeconds(InTimeoutSeconds)
	{
	}

	virtual bool Update() override
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}

		UWorld* World = Weirdplace2E2E::GetPIEWorld();
		AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
		if (World && Char)
		{
			return true;
		}

		if (FPlatformTime::Seconds() - StartTime >= TimeoutSeconds)
		{
			Test->AddError(FString::Printf(
				TEXT("Timed out after %.1fs waiting for PIE player pawn (world=%s, character=%s)"),
				TimeoutSeconds,
				World ? TEXT("ok") : TEXT("null"),
				Char ? TEXT("ok") : TEXT("null")));
			return true;
		}
		return false;
	}

private:
	FAutomationTestBase* Test;
	float TimeoutSeconds;
	double StartTime = 0.0;
};

// Runs an arbitrary one-shot check against the PIE world. Using a lambda per
// check keeps the individual tests readable without needing a bespoke latent
// command class for every assertion stage.
class FWeirdplace2RunCheck : public IAutomationLatentCommand
{
public:
	FWeirdplace2RunCheck(FAutomationTestBase* InTest, TFunction<void(FAutomationTestBase&, UWorld*)> InCheck)
		: Test(InTest)
		, Check(MoveTemp(InCheck))
	{
	}

	virtual bool Update() override
	{
		UWorld* World = Weirdplace2E2E::GetPIEWorld();
		if (Test && Check)
		{
			Check(*Test, World);
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
	TFunction<void(FAutomationTestBase&, UWorld*)> Check;
};

// Drives the player character with a movement vector for a duration so
// CharacterMovement actually integrates velocity across multiple ticks.
// Equivalent to the player holding the forward key for `Duration` seconds.
class FWeirdplace2DriveMovement : public IAutomationLatentCommand
{
public:
	FWeirdplace2DriveMovement(FVector InDirection, float InDurationSeconds)
		: Direction(InDirection)
		, Duration(InDurationSeconds)
	{
	}

	virtual bool Update() override
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}

		UWorld* World = Weirdplace2E2E::GetPIEWorld();
		if (AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World))
		{
			Char->AddMovementInput(Direction, 1.0f, true);
		}
		return (FPlatformTime::Seconds() - StartTime) >= Duration;
	}

private:
	FVector Direction;
	float Duration;
	double StartTime = 0.0;
};

// ---------------------------------------------------------------------------
// Test: Map loads and the player character is present with expected components
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeirdplace2MapLoadsWithPlayerTest,
	"Weirdplace2.E2E.MapLoadsWithPlayer",
	WEIRDPLACE2_E2E_TEST_FLAGS)

bool FWeirdplace2MapLoadsWithPlayerTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(Weirdplace2E2E::DefaultTestMap);

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2WaitForPlayerPawn(this, 10.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2RunCheck(this,
		[](FAutomationTestBase& Test, UWorld* World)
		{
			if (!Test.TestNotNull(TEXT("PIE world exists"), World))
			{
				return;
			}

			AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
			if (!Test.TestNotNull(TEXT("Player character exists"), Char))
			{
				return;
			}

			Test.TestNotNull(TEXT("InventoryComponent attached"), Char->GetInventoryComponent());
			Test.TestNotNull(TEXT("InventoryUIComponent attached"), Char->GetInventoryUIComponent());
			Test.TestNotNull(TEXT("HeldItemComponent attached"), Char->GetHeldItemComponent());

			Test.TestEqual(
				TEXT("Player starts in FreeRoaming activity state"),
				static_cast<uint8>(Char->GetActivityState()),
				static_cast<uint8>(EPlayerActivityState::FreeRoaming));

			Test.TestTrue(TEXT("Player can interact at spawn"), Char->GetCanInteract());
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// ---------------------------------------------------------------------------
// Test: Player moves when movement input is applied
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeirdplace2PlayerCanMoveTest,
	"Weirdplace2.E2E.PlayerCanMove",
	WEIRDPLACE2_E2E_TEST_FLAGS)

bool FWeirdplace2PlayerCanMoveTest::RunTest(const FString& Parameters)
{
	// Captured by latent-command lambdas so stages can share state.
	TSharedRef<FVector> InitialLocation = MakeShared<FVector>(FVector::ZeroVector);

	AutomationOpenMap(Weirdplace2E2E::DefaultTestMap);

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2WaitForPlayerPawn(this, 10.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2RunCheck(this,
		[InitialLocation](FAutomationTestBase& Test, UWorld* World)
		{
			AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
			if (Test.TestNotNull(TEXT("Player character exists"), Char))
			{
				*InitialLocation = Char->GetActorLocation();
			}
		}));

	// Drive the character forward for 1 second — long enough for the
	// CharacterMovementComponent to produce a meaningful position delta.
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2DriveMovement(FVector::ForwardVector, 1.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2RunCheck(this,
		[InitialLocation](FAutomationTestBase& Test, UWorld* World)
		{
			AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
			if (!Test.TestNotNull(TEXT("Player character still present after movement"), Char))
			{
				return;
			}

			const FVector Delta = Char->GetActorLocation() - *InitialLocation;
			const float HorizontalDistance = Delta.Size2D();
			Test.TestTrue(
				*FString::Printf(TEXT("Player moved horizontally at least 10cm (actual=%.1fcm)"), HorizontalDistance),
				HorizontalDistance > 10.0f);
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// ---------------------------------------------------------------------------
// Test: Inventory add/remove/query behaves correctly in-game
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeirdplace2InventoryAddAndQueryTest,
	"Weirdplace2.E2E.InventoryAddAndQuery",
	WEIRDPLACE2_E2E_TEST_FLAGS)

bool FWeirdplace2InventoryAddAndQueryTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(Weirdplace2E2E::DefaultTestMap);

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2WaitForPlayerPawn(this, 10.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2RunCheck(this,
		[](FAutomationTestBase& Test, UWorld* World)
		{
			AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
			if (!Test.TestNotNull(TEXT("Player character exists"), Char))
			{
				return;
			}

			UInventoryComponent* Inv = Char->GetInventoryComponent();
			if (!Test.TestNotNull(TEXT("Inventory component exists"), Inv))
			{
				return;
			}

			const int32 InitialCount = Inv->GetItemCount();
			const FName TestItem(TEXT("E2ETestItem_Alpha"));

			Test.TestFalse(TEXT("Inventory starts without the test item"), Inv->HasItem(TestItem));

			Inv->AddItem(TestItem);
			Test.TestTrue(TEXT("HasItem returns true after AddItem"), Inv->HasItem(TestItem));
			Test.TestEqual(TEXT("Item count grew by one after AddItem"),
				Inv->GetItemCount(), InitialCount + 1);

			const bool bRemoved = Inv->RemoveItem(TestItem);
			Test.TestTrue(TEXT("RemoveItem returns true for existing item"), bRemoved);
			Test.TestFalse(TEXT("HasItem returns false after RemoveItem"), Inv->HasItem(TestItem));
			Test.TestEqual(TEXT("Item count returned to initial after RemoveItem"),
				Inv->GetItemCount(), InitialCount);
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// ---------------------------------------------------------------------------
// Test: Player can interact with a MovieBox and enters inspection state
// ---------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FWeirdplace2MovieBoxInteractionTest,
	"Weirdplace2.E2E.MovieBoxInteraction",
	WEIRDPLACE2_E2E_TEST_FLAGS)

bool FWeirdplace2MovieBoxInteractionTest::RunTest(const FString& Parameters)
{
	AutomationOpenMap(Weirdplace2E2E::DefaultTestMap);

	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(true));
	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2WaitForPlayerPawn(this, 10.0f));
	// The spawner populates MovieBoxes during BeginPlay but the cover-material
	// lookup happens in the first tick or two; give it a little breathing room.
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));

	ADD_LATENT_AUTOMATION_COMMAND(FWeirdplace2RunCheck(this,
		[](FAutomationTestBase& Test, UWorld* World)
		{
			AMyCharacter* Char = Weirdplace2E2E::GetPlayerCharacter(World);
			if (!Test.TestNotNull(TEXT("Player character exists"), Char))
			{
				return;
			}

			// MovieBox::Interact_Implementation early-exits unless inventory is
			// unlocked (gated by Seneca in normal play). Unlock it so this test
			// can exercise interaction without running dialogue first.
			Char->UnlockInventory();
			Test.TestTrue(TEXT("Inventory unlocked for test"), Char->IsInventoryUnlocked());

			AMovieBox* TargetBox = nullptr;
			int32 BoxCount = 0;
			for (TActorIterator<AMovieBox> It(World); It; ++It)
			{
				if (TargetBox == nullptr)
				{
					TargetBox = *It;
				}
				++BoxCount;
			}
			Test.TestTrue(
				*FString::Printf(TEXT("At least one MovieBox exists in level (found %d)"), BoxCount),
				BoxCount > 0);
			if (!TargetBox)
			{
				return;
			}

			// Position the player adjacent to the target so interaction runs
			// against a sensible camera viewpoint (the inspection code offsets
			// the box in front of the player's camera).
			const FVector BoxLocation = TargetBox->GetActorLocation();
			Char->SetActorLocation(BoxLocation + FVector(0.f, 0.f, 200.f));

			IInteractable::Execute_Interact(TargetBox);

			Test.TestEqual(
				TEXT("Player enters Interacting state after IInteractable::Interact"),
				static_cast<uint8>(Char->GetActivityState()),
				static_cast<uint8>(EPlayerActivityState::Interacting));
			Test.TestFalse(TEXT("CanInteract gated off during inspection"), Char->GetCanInteract());

			// Tear down inspection cleanly so the test doesn't leave the
			// player controller with ignored look/move input.
			TargetBox->StopInspection();

			Test.TestEqual(
				TEXT("Player returns to FreeRoaming after StopInspection"),
				static_cast<uint8>(Char->GetActivityState()),
				static_cast<uint8>(EPlayerActivityState::FreeRoaming));
			Test.TestTrue(TEXT("CanInteract restored after StopInspection"), Char->GetCanInteract());
		}));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
