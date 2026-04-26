#pragma once

// All latent command classes (FTD_*) used by the Level1 happy-path E2E test.
// Split out of E2E_Level1Test.cpp so the test body stays readable and tool
// reads stay targeted. Intended to be included by exactly one .cpp — all
// statics (helpers, CVar) are header-local and would double-register if
// included from multiple TUs.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "TestDriverSubsystem.h"
#include "FirstPersonCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TestWaypoint.h"
#include "Door.h"
#include "MovieBox.h"
#include "Hudson.h"
#include "Rick.h"
#include "Seneca.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

// Post-step delay applied after every FTD_Base command finishes. Set to 0 for
// fastest possible runs, or increase to slow the test down for visual review.
// Configurable at runtime via `e2e.StepDelay <seconds>` console command.
static TAutoConsoleVariable<float> CVarE2EStepDelay(
	TEXT("e2e.StepDelay"),
	0.5f,
	TEXT("Seconds to pause after each E2E latent command completes. 0 = no delay."),
	ECVF_Default);

// =======================================================================
// Helpers
// =======================================================================

namespace E2ELatent
{
	inline UWorld* GetPIEWorld()
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

	inline UTestDriverSubsystem* GetDriver()
	{
		UWorld* World = GetPIEWorld();
		return World ? World->GetSubsystem<UTestDriverSubsystem>() : nullptr;
	}
}

using E2ELatent::GetDriver;

// =======================================================================
// FTD_Base — common base for all latent commands. Auto-emits a status line
// via the TestDriver on the first tick using GetStatusText(), then delegates
// to UpdateStep(). Subclasses override GetStatusText() to describe what they
// do; the status pins on the viewport until the next command updates it.
// Returning an empty string skips the status update (useful for Delay).
// =======================================================================

class FTD_Base : public IAutomationLatentCommand
{
public:
	FTD_Base(FAutomationTestBase* InTest = nullptr) : Test(InTest) {}

	virtual bool Update() override final
	{
		// Latent commands are all constructed up-front by ADD_LATENT_AUTOMATION_COMMAND
		// but tick sequentially, so any "elapsed since start" calculation must be
		// anchored on first tick — NOT construction time. Otherwise commands that run
		// late in the queue see huge elapsed values and trip their timeouts immediately.
		if (FirstTickTime == 0.0)
		{
			FirstTickTime = FPlatformTime::Seconds();
		}

		if (!bStatusEmitted)
		{
			const FString Status = GetStatusText();
			if (!Status.IsEmpty())
			{
				if (UTestDriverSubsystem* Driver = GetDriver())
				{
					Driver->SetTestStatus(Status);
				}
			}
			bStatusEmitted = true;
		}

		if (!bStepDone)
		{
			if (!UpdateStep())
			{
				return false;
			}
			bStepDone = true;
			StepDoneTime = FPlatformTime::Seconds();
		}

		// Hold for the globally configured post-step delay so tests can be
		// slowed down for visual review. Skip the wait entirely when 0.
		const float Delay = CVarE2EStepDelay.GetValueOnGameThread();
		if (Delay <= 0.f)
		{
			return true;
		}
		return (FPlatformTime::Seconds() - StepDoneTime) >= Delay;
	}

protected:
	virtual FString GetStatusText() const { return FString(); }
	virtual bool UpdateStep() = 0;

	// Seconds since this command first ticked (not since construction). Use this
	// for all timeout checks so queued commands don't inherit cumulative delay.
	double GetElapsedSinceFirstTick() const
	{
		return FirstTickTime > 0.0 ? (FPlatformTime::Seconds() - FirstTickTime) : 0.0;
	}

	FAutomationTestBase* Test = nullptr;

private:
	double FirstTickTime = 0.0;
	bool bStatusEmitted = false;
	bool bStepDone = false;
	double StepDoneTime = 0.0;
};

// =======================================================================
// FTD_WaitForPlayerReady
// =======================================================================

class FTD_WaitForPlayerReady : public FTD_Base
{
public:
	FTD_WaitForPlayerReady(FAutomationTestBase* InTest, double InTimeoutSeconds = 15.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override { return TEXT("Waiting for player to spawn"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (Driver && Driver->IsPlayerReady())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_WaitForPlayerReady: player never became ready (timeout)"));
			return true;
		}
		return false;
	}

private:
	double Timeout;
};

// =======================================================================
// FTD_Delay — wait N seconds for visual pacing. Intentionally leaves the
// current status line alone (GetStatusText returns empty).
// =======================================================================

class FTD_Delay : public FTD_Base
{
public:
	FTD_Delay(float InSeconds) : Seconds(InSeconds), StartTime(0.0) {}

	virtual bool UpdateStep() override
	{
		if (StartTime == 0.0)
		{
			StartTime = FPlatformTime::Seconds();
		}
		return FPlatformTime::Seconds() - StartTime >= Seconds;
	}
private:
	float Seconds;
	double StartTime;
};

// =======================================================================
// FTD_TeleportTo — teleport to a named waypoint
// =======================================================================

class FTD_TeleportTo : public FTD_Base
{
public:
	FTD_TeleportTo(FAutomationTestBase* InTest, FName InTag) : FTD_Base(InTest), Tag(InTag) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Teleporting to waypoint '%s'"), *Tag.ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportTo: no driver")); return true; }
		if (!Driver->TeleportPlayerToWaypoint(Tag))
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportTo: waypoint '%s' not found"), *Tag.ToString()));
		}
		return true;
	}
private:
	FName Tag;
};

// =======================================================================
// FTD_LookAt* — aim the camera at targets
// =======================================================================

class FTD_LookAtWaypoint : public FTD_Base
{
public:
	FTD_LookAtWaypoint(FAutomationTestBase* InTest, FName InTag) : FTD_Base(InTest), Tag(InTag) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Looking at waypoint '%s'"), *Tag.ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtWaypoint: no driver")); return true; }
		ATestWaypoint* Waypoint = ATestWaypoint::FindByTag(Driver, Tag);
		if (!Waypoint)
		{
			Test->AddError(FString::Printf(TEXT("FTD_LookAtWaypoint: no waypoint '%s'"), *Tag.ToString()));
			return true;
		}
		return Driver->LookAt(Waypoint);
	}
private:
	FName Tag;
};

class FTD_LookAtActorByLabel : public FTD_Base
{
public:
	FTD_LookAtActorByLabel(FAutomationTestBase* InTest, FString InLabel) : FTD_Base(InTest), Label(MoveTemp(InLabel)) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Looking at '%s'"), *Label);
	}

	virtual bool UpdateStep() override
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
	FString Label;
};

// Aim the camera at the world-space position of a named scene component on an
// actor. Needed when the interact-trace target is a sub-component (e.g.
// BP_OutsideBathroomDoor's KeyLockSocket), not the actor's pivot.
class FTD_LookAtActorComponentByName : public FTD_Base
{
public:
	FTD_LookAtActorComponentByName(FAutomationTestBase* InTest, FString InActorLabel, FString InComponentName)
		: FTD_Base(InTest), ActorLabel(MoveTemp(InActorLabel)), ComponentName(MoveTemp(InComponentName)) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Looking at %s.%s"), *ActorLabel, *ComponentName);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtActorComponentByName: no driver")); return true; }
		if (!Driver->LookAtActorComponentByName(ActorLabel, ComponentName))
		{
			Test->AddError(FString::Printf(TEXT("FTD_LookAtActorComponentByName: failed on %s.%s"),
				*ActorLabel, *ComponentName));
		}
		return true;
	}
private:
	FString ActorLabel;
	FString ComponentName;
};

class FTD_LookAtSeneca : public FTD_Base
{
public:
	FTD_LookAtSeneca(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking at Seneca"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtSeneca: no driver")); return true; }
		if (!Driver->LookAtSeneca())
		{
			Test->AddError(TEXT("FTD_LookAtSeneca: failed"));
		}
		return true;
	}
};

// Teleport directly to a position 200 units in front of Seneca, facing her.
// More reliable than the SenecaApproach waypoint, which was placed too far
// away (>500 unit InteractionDistance) for the interact trace to hit.
class FTD_TeleportNearSeneca : public FTD_Base
{
public:
	FTD_TeleportNearSeneca(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting near Seneca"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportNearSeneca: no driver")); return true; }
		ASeneca* Seneca = Driver->FindSeneca();
		if (!Seneca) { Test->AddError(TEXT("FTD_TeleportNearSeneca: no Seneca")); return true; }
		if (!Driver->TeleportNearActor(Seneca, 200.f))
		{
			Test->AddError(TEXT("FTD_TeleportNearSeneca: teleport failed"));
		}
		return true;
	}
};

// Teleport to `Distance` units in front of an actor found by editor label,
// facing it. Handy when a waypoint is too far away for the interact trace.
class FTD_TeleportNearActorByLabel : public FTD_Base
{
public:
	FTD_TeleportNearActorByLabel(FAutomationTestBase* InTest, FString InLabel, float InDistance = 200.f)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Distance(InDistance) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Teleporting near '%s'"), *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportNearActorByLabel: no driver")); return true; }
		AActor* Target = Driver->FindActorByLabel(Label);
		if (!Target)
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportNearActorByLabel: no actor '%s'"), *Label));
			return true;
		}
		if (!Driver->TeleportNearActor(Target, Distance))
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportNearActorByLabel: teleport to '%s' failed"), *Label));
		}
		return true;
	}
private:
	FString Label;
	float Distance;
};

class FTD_TeleportNearRick : public FTD_Base
{
public:
	FTD_TeleportNearRick(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting near Rick"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportNearRick: no driver")); return true; }
		ARick* Rick = Driver->FindRick();
		if (!Rick) { Test->AddError(TEXT("FTD_TeleportNearRick: no Rick")); return true; }
		if (!Driver->TeleportNearActor(Rick, 200.f))
		{
			Test->AddError(TEXT("FTD_TeleportNearRick: teleport failed"));
		}
		return true;
	}
};

class FTD_LookAtRick : public FTD_Base
{
public:
	FTD_LookAtRick(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking at Rick"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtRick: no driver")); return true; }
		if (!Driver->LookAtRick())
		{
			Test->AddError(TEXT("FTD_LookAtRick: failed"));
		}
		return true;
	}
};

class FTD_LookAtKeyActor : public FTD_Base
{
public:
	FTD_LookAtKeyActor(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking at the key"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtKeyActor: no driver")); return true; }
		if (!Driver->LookAtKeyActor())
		{
			Test->AddError(TEXT("FTD_LookAtKeyActor: failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_SimulateKeyPress — press+release a key with a 1-frame gap.
// Uses APlayerController::InputKey, which only fires LEGACY input bindings.
// For Enhanced Input actions, use FTD_SimulateInteractAction /
// FTD_SimulateInventoryAction below.
// =======================================================================

class FTD_SimulateKeyPress : public FTD_Base
{
public:
	FTD_SimulateKeyPress(FAutomationTestBase* InTest, FKey InKey)
		: FTD_Base(InTest), Key(InKey), bPressed(false) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Pressing %s"), *Key.GetDisplayName().ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SimulateKeyPress: no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateKeyPress(Key);
			bPressed = true;
			return false; // wait one frame
		}

		Driver->SimulateKeyRelease(Key);
		return true;
	}
private:
	FKey Key;
	bool bPressed;
};

// =======================================================================
// FTD_SimulateInteractAction — inject the InteractAction (E key) through
// Enhanced Input. Press → 1 frame gap → Release so the do-once gate resets.
// =======================================================================

class FTD_SimulateInteractAction : public FTD_Base
{
public:
	FTD_SimulateInteractAction(FAutomationTestBase* InTest) : FTD_Base(InTest), bPressed(false) {}

	virtual FString GetStatusText() const override { return TEXT("Pressing E to interact"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SimulateInteractAction: no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateInteractPress();
			bPressed = true;
			return false;
		}
		Driver->SimulateInteractRelease();
		return true;
	}
private:
	bool bPressed;
};

// =======================================================================
// FTD_SimulateInventoryAction — inject the InventoryAction (Tab) via
// Enhanced Input.
// =======================================================================

class FTD_SimulateInventoryAction : public FTD_Base
{
public:
	FTD_SimulateInventoryAction(FAutomationTestBase* InTest) : FTD_Base(InTest), bPressed(false) {}

	virtual FString GetStatusText() const override { return TEXT("Pressing Tab for inventory"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SimulateInventoryAction: no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateInventoryPress();
			bPressed = true;
			return false;
		}
		Driver->SimulateInventoryRelease();
		return true;
	}
private:
	bool bPressed;
};

// =======================================================================
// FTD_WaitForActivityState
// =======================================================================

class FTD_WaitForActivityState : public FTD_Base
{
public:
	FTD_WaitForActivityState(FAutomationTestBase* InTest, EPlayerActivityState InExpected, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Expected(InExpected), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for activity state %d"), (int32)Expected);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (Driver->GetActivityState() == Expected)
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_WaitForActivityState: timed out waiting for state %d (current %d)"),
				(int32)Expected, (int32)Driver->GetActivityState()));
			return true;
		}
		return false;
	}
private:
	EPlayerActivityState Expected;
	double Timeout;
};

// =======================================================================
// FTD_AdvanceDialogueViaInput — press E repeatedly to advance dialogue
// until target activity state is reached, with inter-line delay.
// =======================================================================

class FTD_AdvanceDialogueViaInput : public FTD_Base
{
public:
	FTD_AdvanceDialogueViaInput(FAutomationTestBase* InTest, EPlayerActivityState InTarget,
		double InLineDelay = 1.0, double InTimeoutSeconds = 30.0)
		: FTD_Base(InTest), Target(InTarget), LineDelay(InLineDelay)
		, Timeout(InTimeoutSeconds)
		, LastPressTime(0.0), bWaitingForRelease(false) {}

	virtual FString GetStatusText() const override { return TEXT("Advancing dialogue with E"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		const EPlayerActivityState State = Driver->GetActivityState();

		if (State == Target)
		{
			return true;
		}

		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AdvanceDialogueViaInput: timed out in state %d"), (int32)State));
			return true;
		}

		const bool bInDialogue =
			State == EPlayerActivityState::InSimpleDialogue ||
			State == EPlayerActivityState::InDialogue;

		if (!bInDialogue)
		{
			// Not yet in dialogue and not at target — wait for dialogue to start.
			return false;
		}

		// Handle press/release cycle with inter-line delay.
		const double Now = FPlatformTime::Seconds();

		if (bWaitingForRelease)
		{
			Driver->SimulateInteractRelease();
			bWaitingForRelease = false;
			LastPressTime = Now;
			return false;
		}

		if (Now - LastPressTime < LineDelay)
		{
			return false; // wait for inter-line delay
		}

		Driver->SimulateInteractPress();
		bWaitingForRelease = true;
		return false;
	}
private:
	EPlayerActivityState Target;
	double LineDelay;
	double Timeout;
	double LastPressTime;
	bool bWaitingForRelease;
};

// =======================================================================
// FTD_AdvanceDialogueUntilItemNotification — press E repeatedly until
// the ItemNotificationMesh becomes visible (item was given mid-dialogue).
// =======================================================================

class FTD_AdvanceDialogueUntilItemNotification : public FTD_Base
{
public:
	FTD_AdvanceDialogueUntilItemNotification(FAutomationTestBase* InTest,
		double InLineDelay = 1.0, double InTimeoutSeconds = 30.0)
		: FTD_Base(InTest), LineDelay(InLineDelay), Timeout(InTimeoutSeconds)
		, LastPressTime(0.0), bWaitingForRelease(false) {}

	virtual FString GetStatusText() const override { return TEXT("Advancing dialogue until item notification appears"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (Player && Player->IsItemNotificationVisible())
		{
			if (bWaitingForRelease)
			{
				Driver->SimulateInteractRelease();
				bWaitingForRelease = false;
			}
			return true;
		}

		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_AdvanceDialogueUntilItemNotification: timed out"));
			return true;
		}

		const EPlayerActivityState State = Driver->GetActivityState();
		const bool bInDialogue =
			State == EPlayerActivityState::InSimpleDialogue ||
			State == EPlayerActivityState::InDialogue;

		if (!bInDialogue)
		{
			return false;
		}

		const double Now = FPlatformTime::Seconds();

		if (bWaitingForRelease)
		{
			Driver->SimulateInteractRelease();
			bWaitingForRelease = false;
			LastPressTime = Now;
			return false;
		}

		if (Now - LastPressTime < LineDelay)
		{
			return false;
		}

		Driver->SimulateInteractPress();
		bWaitingForRelease = true;
		return false;
	}
private:
	double LineDelay;
	double Timeout;
	double LastPressTime;
	bool bWaitingForRelease;
};

// =======================================================================
// FTD_OpenInventoryViaInput — press Tab, wait for fully open
// =======================================================================

class FTD_OpenInventoryViaInput : public FTD_Base
{
public:
	FTD_OpenInventoryViaInput(FAutomationTestBase* InTest, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds)
		, bPressed(false), bReleased(false) {}

	virtual FString GetStatusText() const override { return TEXT("Opening inventory (Tab)"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateInventoryPress();
			bPressed = true;
			return false;
		}
		if (!bReleased)
		{
			Driver->SimulateInventoryRelease();
			bReleased = true;
			return false;
		}

		if (Driver->IsInventoryFullyOpen())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_OpenInventoryViaInput: timed out waiting for inventory to open"));
			return true;
		}
		return false;
	}
private:
	double Timeout;
	bool bPressed;
	bool bReleased;
};

// =======================================================================
// FTD_CloseInventoryViaInput — press Tab, wait for fully closed
// =======================================================================

class FTD_CloseInventoryViaInput : public FTD_Base
{
public:
	FTD_CloseInventoryViaInput(FAutomationTestBase* InTest, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds)
		, bPressed(false), bReleased(false) {}

	virtual FString GetStatusText() const override { return TEXT("Closing inventory (Tab)"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateInventoryPress();
			bPressed = true;
			return false;
		}
		if (!bReleased)
		{
			Driver->SimulateInventoryRelease();
			bReleased = true;
			return false;
		}

		if (Driver->IsInventoryFullyClosed())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_CloseInventoryViaInput: timed out waiting for inventory to close"));
			return true;
		}
		return false;
	}
private:
	double Timeout;
	bool bPressed;
	bool bReleased;
};

// =======================================================================
// FTD_SelectAndConfirmSlot — set cursor to slot N, press E to confirm
// =======================================================================

class FTD_SelectAndConfirmSlot : public FTD_Base
{
public:
	FTD_SelectAndConfirmSlot(FAutomationTestBase* InTest, int32 InIndex)
		: FTD_Base(InTest), Index(InIndex), Phase(0) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Selecting inventory slot %d, pressing E"), Index);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		switch (Phase)
		{
		case 0: // Set the cursor position
			if (!Driver->SetSelectedSlot(Index))
			{
				Test->AddError(FString::Printf(TEXT("FTD_SelectAndConfirmSlot: failed to set slot %d"), Index));
				return true;
			}
			Phase = 1;
			return false;
		case 1: // Press E — fires legacy "InventoryConfirmSelection" via InputKey
			Driver->SimulateKeyPress(EKeys::E);
			Phase = 2;
			return false;
		case 2: // Release E
			Driver->SimulateKeyRelease(EKeys::E);
			return true;
		default:
			return true;
		}
	}
private:
	int32 Index;
	int32 Phase;
};

// =======================================================================
// FTD_TeleportNearAndLookAtMovie — find next uncollected movie,
// teleport near it, and aim the camera at it.
// =======================================================================

class FTD_TeleportNearAndLookAtMovie : public FTD_Base
{
public:
	FTD_TeleportNearAndLookAtMovie(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting to next movie"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		AMovieBox* Movie = Driver->FindNextUncollectedMovie();
		if (!Movie)
		{
			Test->AddError(TEXT("FTD_TeleportNearAndLookAtMovie: no uncollected movie"));
			return true;
		}

		if (!Driver->TeleportNearActor(Movie, 200.f))
		{
			Test->AddError(TEXT("FTD_TeleportNearAndLookAtMovie: teleport failed"));
			return true;
		}

		if (!Driver->LookAt(Movie))
		{
			Test->AddError(TEXT("FTD_TeleportNearAndLookAtMovie: LookAt failed"));
			return true;
		}
		return true;
	}
};

// =======================================================================
// FTD_RotateAndCollectMovie — inject MouseX to rotate the inspected
// movie box until it's collected (activity state returns to FreeRoaming).
// =======================================================================

class FTD_RotateAndCollectMovie : public FTD_Base
{
public:
	FTD_RotateAndCollectMovie(FAutomationTestBase* InTest, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds)
		, FrameCount(0), bCollectPressed(false) {}

	virtual FString GetStatusText() const override { return TEXT("Rotating movie and pressing E to collect"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (Driver->GetActivityState() == EPlayerActivityState::FreeRoaming)
		{
			// Collection completed — StopInspection set us back to FreeRoaming.
			Driver->MarkLastFoundMovieCollected();
			return true;
		}

		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_RotateAndCollectMovie: timed out"));
			return true;
		}

		// Inject mouse rotation each frame. 5.0 axis units * 2.0 deg/unit = 10 deg/frame.
		// After ~18 frames we've rotated 180 degrees, which should cross the 0.9 dot threshold.
		Driver->SimulateMouseX(90.0f);
		FrameCount++;

		// After enough rotation, try pressing E to collect.
		// Need ~155 degrees of rotation (at 10 deg/frame = ~16 frames).
		// The "Collect Inspected Subitem" binding is only active when dot > 0.9,
		// so early presses are harmless (no binding = no effect).
		if (FrameCount >= 2 && !bCollectPressed)
		{
			Driver->SimulateKeyPress(EKeys::E);
			bCollectPressed = true;
		}
		else if (bCollectPressed)
		{
			Driver->SimulateKeyRelease(EKeys::E);
			bCollectPressed = false; // allow retry next frame
		}

		return false;
	}
private:
	double Timeout;
	int32 FrameCount;
	bool bCollectPressed;
};

// =======================================================================
// Screenshots.
// =======================================================================

class FTD_TakeScreenshot : public FTD_Base
{
public:
	FTD_TakeScreenshot(const FString& InName)
		: Name(InName), bRequested(false) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Taking screenshot '%s'"), *Name);
	}

	virtual bool UpdateStep() override
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

class FTD_AssertInventoryCount : public FTD_Base
{
public:
	FTD_AssertInventoryCount(FAutomationTestBase* InTest, int32 InExpected) : FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting inventory count == %d"), Expected);
	}

	virtual bool UpdateStep() override
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
	int32 Expected;
};

class FTD_AssertHasItem : public FTD_Base
{
public:
	FTD_AssertHasItem(FAutomationTestBase* InTest, FName InItemId) : FTD_Base(InTest), ItemId(InItemId) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting player has item '%s'"), *ItemId.ToString());
	}

	virtual bool UpdateStep() override
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
	FName ItemId;
};

class FTD_AssertNotHasItem : public FTD_Base
{
public:
	FTD_AssertNotHasItem(FAutomationTestBase* InTest, FName InItemId) : FTD_Base(InTest), ItemId(InItemId) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting player does NOT have item '%s'"), *ItemId.ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (Driver->HasItem(ItemId))
		{
			Test->AddError(FString::Printf(TEXT("AssertNotHasItem: '%s' should NOT be in inventory"), *ItemId.ToString()));
		}
		return true;
	}
private:
	FName ItemId;
};

// =======================================================================
// FTD_WaitForItemAdded — poll HasItem(Id) until it returns true.
// Useful for waiting on async item grants (e.g. the key-break timeline
// chain adds "BrokenKey" a few seconds after interact).
// =======================================================================

class FTD_WaitForItemAdded : public FTD_Base
{
public:
	FTD_WaitForItemAdded(FAutomationTestBase* InTest, FName InItemId, double InTimeoutSeconds = 8.0)
		: FTD_Base(InTest), ItemId(InItemId), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for item '%s' to be added"), *ItemId.ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (Driver->HasItem(ItemId))
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_WaitForItemAdded: timed out waiting for '%s'"), *ItemId.ToString()));
			return true;
		}
		return false;
	}
private:
	FName ItemId;
	double Timeout;
};

// =======================================================================
// FTD_FastForwardSenecaSmoking — skip Seneca's 60s SmokingAppearDelay
// timer. One-shot.
// =======================================================================

class FTD_FastForwardSenecaSmoking : public FTD_Base
{
public:
	FTD_FastForwardSenecaSmoking(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Fast-forwarding Seneca smoking delay"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		Driver->FastForwardSenecaSmoking();
		return true;
	}
};

// =======================================================================
// FTD_WaitForSenecaAppearedAtSmoking — poll until Seneca has been
// re-teleported out of the hidden below-world position back to the
// smoking spot. Depends on the SenecaSmoking waypoint being placed so
// its default facing does NOT look at SmokingPositionTarget.
// =======================================================================

class FTD_WaitForSenecaAppearedAtSmoking : public FTD_Base
{
public:
	FTD_WaitForSenecaAppearedAtSmoking(FAutomationTestBase* InTest, double InTimeoutSeconds = 3.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override { return TEXT("Waiting for Seneca to appear at smoking spot"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		if (Driver->HasSenecaAppearedAtSmokingPos())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_WaitForSenecaAppearedAtSmoking: timed out (check SenecaSmoking waypoint facing)"));
			return true;
		}
		return false;
	}
private:
	double Timeout;
};

// =======================================================================
// FTD_WaitForSenecaState — poll Seneca->CurrentState until it matches
// the expected ESenecaState value.
// =======================================================================

class FTD_WaitForSenecaState : public FTD_Base
{
public:
	FTD_WaitForSenecaState(FAutomationTestBase* InTest, ESenecaState InExpected, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Expected(InExpected), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for Seneca state %d"), (int32)Expected);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		ASeneca* Seneca = Driver->FindSeneca();
		if (!Seneca) { Test->AddError(TEXT("FTD_WaitForSenecaState: no Seneca")); return true; }
		if (Seneca->CurrentState == Expected)
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_WaitForSenecaState: timed out waiting for %d (current %d)"),
				(int32)Expected, (int32)Seneca->CurrentState));
			return true;
		}
		return false;
	}
private:
	ESenecaState Expected;
	double Timeout;
};

// =======================================================================
// FTD_WaitForDoorOpen — find a door by editor label and poll IsOpen()
// until the door timeline finishes.
// =======================================================================

class FTD_WaitForDoorOpen : public FTD_Base
{
public:
	FTD_WaitForDoorOpen(FAutomationTestBase* InTest, FString InLabel, double InTimeoutSeconds = 3.0)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for door '%s' to open"), *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }
		AActor* Actor = Driver->FindActorByLabel(Label);
		ADoor* Door = Cast<ADoor>(Actor);
		if (!Door)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForDoorOpen: no ADoor with label '%s'"), *Label));
			return true;
		}
		if (Door->IsOpen())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForDoorOpen: '%s' never opened"), *Label));
			return true;
		}
		return false;
	}
private:
	FString Label;
	double Timeout;
};

// =======================================================================
// FTD_LerpTo — smoothly noclip the player from their current position
// to a waypoint over a given duration. Collision and the movement
// component are disabled for the move so the player passes through walls.
// =======================================================================

class FTD_LerpTo : public FTD_Base
{
public:
	FTD_LerpTo(FAutomationTestBase* InTest, FName InTag, float InDuration)
		: FTD_Base(InTest), Tag(InTag), Duration(InDuration)
		, bInitialized(false) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Lerping to waypoint '%s' over %.1fs"), *Tag.ToString(), Duration);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LerpTo: no driver")); return true; }

		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_LerpTo: no player")); return true; }

		if (!bInitialized)
		{
			ATestWaypoint* Waypoint = ATestWaypoint::FindByTag(Player, Tag);
			if (!Waypoint)
			{
				Test->AddError(FString::Printf(TEXT("FTD_LerpTo: no waypoint '%s'"), *Tag.ToString()));
				return true;
			}
			StartPos = Player->GetActorLocation();
			EndPos = Waypoint->GetActorLocation();
			// Keep player at their current Z so we walk flat, not float up/down
			EndPos.Z = StartPos.Z;
			Player->SetActorEnableCollision(false);
			// Stop the movement component so it doesn't fight our position updates
			Player->GetCharacterMovement()->SetMovementMode(MOVE_None);
			bInitialized = true;

			if (Duration <= 0.f)
			{
				Player->SetActorLocation(EndPos, false, nullptr, ETeleportType::TeleportPhysics);
				Player->SetActorEnableCollision(true);
				Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
				return true;
			}
		}

		const float Alpha = FMath::Clamp(static_cast<float>(GetElapsedSinceFirstTick()) / Duration, 0.f, 1.f);
		const FVector NewPos = FMath::Lerp(StartPos, EndPos, Alpha);
		Player->SetActorLocation(NewPos, false, nullptr, ETeleportType::TeleportPhysics);

		if (Alpha >= 1.f)
		{
			Player->SetActorEnableCollision(true);
			Player->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			return true;
		}
		return false;
	}

private:
	FName Tag;
	float Duration;
	bool bInitialized;
	FVector StartPos;
	FVector EndPos;
};

// =======================================================================
// FTD_TeleportNearHudson
// =======================================================================

class FTD_TeleportNearHudson : public FTD_Base
{
public:
	FTD_TeleportNearHudson(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting near Hudson"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportNearHudson: no driver")); return true; }
		AHudson* Hudson = Driver->FindHudson();
		if (!Hudson) { Test->AddError(TEXT("FTD_TeleportNearHudson: no Hudson")); return true; }
		if (!Driver->TeleportNearActor(Hudson, 200.f))
		{
			Test->AddError(TEXT("FTD_TeleportNearHudson: teleport failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_LookAtHudson
// =======================================================================

class FTD_LookAtHudson : public FTD_Base
{
public:
	FTD_LookAtHudson(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking at Hudson"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtHudson: no driver")); return true; }
		if (!Driver->LookAtHudson())
		{
			Test->AddError(TEXT("FTD_LookAtHudson: failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_AssertActivityState — instant assertion (NOT a wait). Fails the
// test if the current state doesn't match Expected.
// =======================================================================

// =======================================================================
// FTD_SetGamepadLookSensitivity — write directly to the persisted settings
// (clamped + snapped). Used by sensitivity diagnostic tests.
// =======================================================================

class FTD_SetGamepadLookSensitivity : public FTD_Base
{
public:
	FTD_SetGamepadLookSensitivity(FAutomationTestBase* InTest, float InValue)
		: FTD_Base(InTest), Value(InValue) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Setting gamepad look sensitivity to %.3f"), Value);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SetGamepadLookSensitivity: no driver")); return true; }
		Driver->SetGamepadLookSensitivity(Value);
		return true;
	}
private:
	float Value;
};

// =======================================================================
// FTD_CaptureYaw — record the current ControlRotation.Yaw to a caller-owned
// float so a later FTD_AssertYawDelta can compare against it.
// =======================================================================

class FTD_CaptureYaw : public FTD_Base
{
public:
	FTD_CaptureYaw(FAutomationTestBase* InTest, float* InOutYaw)
		: FTD_Base(InTest), OutYaw(InOutYaw) {}

	virtual FString GetStatusText() const override { return TEXT("Capturing control yaw"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver || !OutYaw) { Test->AddError(TEXT("FTD_CaptureYaw: missing driver/yaw")); return true; }
		*OutYaw = Driver->GetControllerYaw();
		UE_LOG(LogTemp, Log, TEXT("FTD_CaptureYaw: yaw=%.3f"), *OutYaw);
		return true;
	}
private:
	float* OutYaw;
};

// =======================================================================
// FTD_AssertYawDelta — log the absolute yaw delta from a previously captured
// yaw, and fail if it's outside [MinAbsDelta, MaxAbsDelta].
// =======================================================================

class FTD_AssertYawDelta : public FTD_Base
{
public:
	FTD_AssertYawDelta(FAutomationTestBase* InTest, FString InLabel, float* InCapturedYaw,
		float InMinAbsDelta, float InMaxAbsDelta)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), CapturedYaw(InCapturedYaw)
		, MinAbsDelta(InMinAbsDelta), MaxAbsDelta(InMaxAbsDelta) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting |yaw delta| '%s' in [%.3f, %.3f]"),
			*Label, MinAbsDelta, MaxAbsDelta);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver || !CapturedYaw) { Test->AddError(TEXT("FTD_AssertYawDelta: missing driver/yaw")); return true; }

		const float Now = Driver->GetControllerYaw();
		float Delta = Now - *CapturedYaw;
		while (Delta > 180.0f) Delta -= 360.0f;
		while (Delta < -180.0f) Delta += 360.0f;
		const float Abs = FMath::Abs(Delta);

		UE_LOG(LogTemp, Warning,
			TEXT("YawDelta[%s]: actual=%.4f deg (|abs|=%.4f, expected range [%.4f, %.4f])"),
			*Label, Delta, Abs, MinAbsDelta, MaxAbsDelta);

		if (Abs < MinAbsDelta || Abs > MaxAbsDelta)
		{
			Test->AddError(FString::Printf(
				TEXT("YawDelta[%s] OUT OF RANGE: |actual|=%.4f deg, expected [%.4f, %.4f]"),
				*Label, Abs, MinAbsDelta, MaxAbsDelta));
		}
		return true;
	}
private:
	FString Label;
	float* CapturedYaw;
	float MinAbsDelta;
	float MaxAbsDelta;
};

// =======================================================================
// FTD_InjectMouseXForDuration — call SimulateMouseX(Delta) every tick for
// DurationSeconds. Drives the LookAction through the legacy mouse-axis path
// (which the IMC's MouseX binding picks up).
// =======================================================================

class FTD_InjectMouseXForDuration : public FTD_Base
{
public:
	FTD_InjectMouseXForDuration(FAutomationTestBase* InTest, float InDelta, float InDurationSeconds)
		: FTD_Base(InTest), Delta(InDelta), DurationSeconds(InDurationSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Injecting MouseX %.1f for %.2fs"), Delta, DurationSeconds);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_InjectMouseXForDuration: no driver")); return true; }
		Driver->SimulateMouseX(Delta);
		return GetElapsedSinceFirstTick() >= DurationSeconds;
	}
private:
	float Delta;
	float DurationSeconds;
};

class FTD_AssertActivityState : public FTD_Base
{
public:
	FTD_AssertActivityState(FAutomationTestBase* InTest, EPlayerActivityState InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting activity state == %d"), (int32)Expected);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertActivityState: no driver")); return true; }

		EPlayerActivityState Actual = Driver->GetActivityState();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertActivityState: expected state %d but got %d"),
				(int32)Expected, (int32)Actual));
		}
		return true;
	}
private:
	EPlayerActivityState Expected;
};

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
