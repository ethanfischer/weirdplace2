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
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TestWaypoint.h"
#include "Door.h"
#include "MovieBox.h"
#include "InspectablePickup.h"
#include "Hudson.h"
#include "Rick.h"
#include "Seneca.h"
#include "MenuUIComponent.h"
#include "MenuUIActor.h"
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

// Teleport directly in front of the chord-spawned blank tape using the box's
// own forward vector. The chord spawner picks one of many top-shelf boxes
// (random bookcase/shelf position) and replaces it, so a static waypoint
// won't reach reliably. The spawner's ChosenForwardOffset translates the
// chosen tape along +ForwardVector to make it stick out from the shelf —
// meaning +ForwardVector always points out toward the viewer, regardless of
// which bookcase the random pick landed on. Approach from that direction so
// the interact raycast has an unobstructed path.
class FTD_TeleportNearBlankTape : public FTD_Base
{
public:
	// 80cm default: close enough that the eye-line into the shelf slot clears
	// the plank above the box, like a player leaning in.
	FTD_TeleportNearBlankTape(FAutomationTestBase* InTest, float InDistance = 80.f)
		: FTD_Base(InTest), Distance(InDistance) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting in front of blank tape"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportNearBlankTape: no driver")); return true; }
		AMovieBox* Tape = Driver->FindBlankTape();
		if (!Tape) { Test->AddError(TEXT("FTD_TeleportNearBlankTape: no blank tape")); return true; }
		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_TeleportNearBlankTape: no player")); return true; }

		const FVector TapeLoc = Tape->GetActorLocation();
		// Aim at the envelope's own bounds — the actor bounds center is skewed
		// by the Tape child mesh, enough to slip the trace past the envelope's
		// collision at some shelf-slot angles.
		USceneComponent* Envelope = Driver->FindComponentOnActorByName(Tape, TEXT("Cube"));
		if (!Envelope) { Test->AddError(TEXT("FTD_TeleportNearBlankTape: blank tape has no 'Cube' envelope")); return true; }
		const FVector TapeCenter = Envelope->Bounds.Origin;
		FVector Forward = Tape->GetActorForwardVector();
		Forward.Z = 0.f;
		Forward = Forward.GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::ForwardVector;
		}

		const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

		// The blank inherits the replaced slot's rotation, and on some racks
		// that faces INTO the shelf unit — standing "in front" there means the
		// next aisle, where the interact trace hits that rack's own boxes.
		// Stand on whichever side can actually trace to the tape, using the
		// same object types as RaycastInteractableCheck.
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel6);

		UE_LOG(LogTemp, Log, TEXT("FTD_TeleportNearBlankTape: tape loc=%s boundsCenter=%s fwd=%s"),
			*TapeLoc.ToString(), *TapeCenter.ToString(), *Forward.ToString());

		bool bFoundSpot = false;
		FVector NewLoc = FVector::ZeroVector;
		Driver->BlankTapeAimPoint = FVector::ZeroVector;
		for (const float Sign : { 1.f, -1.f })
		{
			const FVector TryLoc = TapeLoc + Forward * (Distance * Sign) + FVector(0.f, 0.f, HalfHeight);
			// Probe from where the camera actually ends up after landing
			// (~42cm above the shelf slot at this approach distance).
			const FVector Eye(TryLoc.X, TryLoc.Y, TapeCenter.Z + 42.f);
			FHitResult Hit;
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BlankTapeAim), /*bTraceComplex*/ false);
			QueryParams.AddIgnoredActor(Player);
			const bool bHit = Tape->GetWorld()->LineTraceSingleByObjectType(Hit, Eye, TapeCenter, ObjectParams, QueryParams);
			UE_LOG(LogTemp, Log, TEXT("FTD_TeleportNearBlankTape: side %+.0f eye=%s -> hit %s at %s"),
				Sign, *Eye.ToString(),
				bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(nothing)"),
				bHit ? *Hit.ImpactPoint.ToString() : TEXT("-"));
			if (bHit && Hit.GetActor() == Tape)
			{
				NewLoc = TryLoc;
				// Hand the verified-hittable surface point (nudged just inside
				// the collision) to FTD_LookAtBlankTape — derived bounds
				// centers miss the Memphis mesh's collision at some angles.
				Driver->BlankTapeAimPoint = Hit.ImpactPoint + (TapeCenter - Eye).GetSafeNormal() * 3.f;
				bFoundSpot = true;
				break;
			}
		}
		if (!bFoundSpot)
		{
			Test->AddError(TEXT("FTD_TeleportNearBlankTape: neither side of the tape has a clear interact trace to it"));
			return true;
		}

		Player->SetActorLocation(NewLoc, false, nullptr, ETeleportType::TeleportPhysics);

		const FRotator LookRot = (Driver->BlankTapeAimPoint - NewLoc).Rotation();
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			PC->SetControlRotation(LookRot);
		}
		return true;
	}
private:
	float Distance;
};

class FTD_LookAtBlankTape : public FTD_Base
{
public:
	FTD_LookAtBlankTape(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking at blank tape"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookAtBlankTape: no driver")); return true; }
		AMovieBox* Tape = Driver->FindBlankTape();
		if (!Tape) { Test->AddError(TEXT("FTD_LookAtBlankTape: no blank tape")); return true; }
		// Aim at the surface point FTD_TeleportNearBlankTape verified is
		// hittable — every derived center (actor bounds, envelope bounds)
		// misses the Memphis mesh's collision at some shelf-slot angles.
		if (Driver->BlankTapeAimPoint.IsZero())
		{
			Test->AddError(TEXT("FTD_LookAtBlankTape: run FTD_TeleportNearBlankTape first (no verified aim point)"));
			return true;
		}
		if (!Driver->LookAtWorldPoint(Driver->BlankTapeAimPoint))
		{
			Test->AddError(TEXT("FTD_LookAtBlankTape: LookAt failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_SweepGazeOverBlankVhs — diagnostic. Aims the camera reticle at a grid
// of points across the blank VHS's front face (the 4 corners are the grid
// extremes) and, at each, logs exactly what the chord gaze trace landed on
// (looking?, hit actor, hit component, distance, chord volume). Gathers
// objective data on why the look-to-boost is spotty across the surface.
// Asserts the 4 corners register the gaze, so it also guards regressions.
// =======================================================================
class FTD_SweepGazeOverBlankVhs : public FTD_Base
{
public:
	FTD_SweepGazeOverBlankVhs(FAutomationTestBase* InTest, int32 InGridN = 5,
		const FString& InShotPrefix = TEXT("E2E_GazeSweep"))
		: FTD_Base(InTest), GridN(FMath::Max(2, InGridN)), ShotPrefix(InShotPrefix) {}

	virtual FString GetStatusText() const override { return TEXT("Sweeping gaze across blank VHS surface"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SweepGazeOverBlankVhs: no driver")); return true; }

		if (!bInitialized)
		{
			if (!BuildGrid(Driver)) { return true; }
			bInitialized = true;
			Phase = EPhase::Aim;
			return false;
		}

		switch (Phase)
		{
		case EPhase::Aim:
			Driver->LookAtWorldPoint(Points[Index]);
			SettleFrames = 0;
			Phase = EPhase::Settle;
			return false;

		case EPhase::Settle:
			// Let the control rotation apply and the spawner component re-trace.
			if (++SettleFrames < 3) { return false; }
			Phase = EPhase::Sample;
			return false;

		case EPhase::Sample:
		{
			bool bHasChosen = false, bLooking = false, bHadHit = false;
			FString HitActor, HitComp; float Dist = -1.f, Vol = 0.f; FVector Impact;
			Driver->GetBlankVhsGazeState(bHasChosen, bLooking, bHadHit, HitActor, HitComp, Dist, Impact, Vol);

			const int32 Col = Index % GridN, Row = Index / GridN;
			const bool bCorner = IsCorner(Index);
			UE_LOG(LogTemp, Warning,
				TEXT("GazeSweep[%02d] col=%d row=%d%s aim=%s looking=%d hitActor=%s hitComp=%s dist=%.1f vol=%.2f"),
				Index, Col, Row, bCorner ? TEXT(" CORNER") : TEXT(""), *Points[Index].ToString(),
				bLooking ? 1 : 0, *HitActor, *HitComp, Dist, Vol);

			if (bLooking) { ++LookingHits; }
			else if (bCorner) { MissedCorners.Add(Index); }

			if (bCorner && !FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
			{
				FScreenshotRequest::RequestScreenshot(
					FString::Printf(TEXT("%s_c%d_r%d"), *ShotPrefix, Col, Row), false, false);
				ShotFrames = 0;
				Phase = EPhase::ShotWait;
				return false;
			}
			Phase = EPhase::Advance;
			return false;
		}

		case EPhase::ShotWait:
			if (!FScreenshotRequest::IsScreenshotRequested() || ++ShotFrames > 120)
			{
				if (ShotFrames > 120) { FScreenshotRequest::Reset(); }
				Phase = EPhase::Advance;
			}
			return false;

		case EPhase::Advance:
			if (++Index >= Points.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("GazeSweep SUMMARY: %d/%d points registered the gaze; %d corner(s) missed"),
					LookingHits, Points.Num(), MissedCorners.Num());
				if (MissedCorners.Num() > 0)
				{
					FString Ids;
					for (int32 I : MissedCorners) { Ids += FString::Printf(TEXT("%d "), I); }
					Test->AddError(FString::Printf(
						TEXT("FTD_SweepGazeOverBlankVhs: %d corner(s) did not register the gaze (indices: %s) — collision does not cover the full surface"),
						MissedCorners.Num(), *Ids));
				}
				return true;
			}
			Phase = EPhase::Aim;
			return false;
		}
		return true;
	}

private:
	enum class EPhase { Aim, Settle, Sample, ShotWait, Advance };

	bool IsCorner(int32 i) const
	{
		const int32 c = i % GridN, r = i / GridN;
		return (c == 0 || c == GridN - 1) && (r == 0 || r == GridN - 1);
	}

	// Build a grid of aim points across the blank VHS's player-facing face,
	// using its envelope (Cube) world AABB. The face is the AABB side toward
	// the camera; the grid spans the other two world axes.
	bool BuildGrid(UTestDriverSubsystem* Driver)
	{
		AMovieBox* Tape = Driver->FindBlankTape();
		if (!Tape) { Test->AddError(TEXT("FTD_SweepGazeOverBlankVhs: no blank tape")); return false; }
		USceneComponent* Env = Driver->FindComponentOnActorByName(Tape, TEXT("Cube"));
		if (!Env) { Test->AddError(TEXT("FTD_SweepGazeOverBlankVhs: blank tape has no 'Cube' envelope")); return false; }
		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player || !Player->GetFirstPersonCamera()) { Test->AddError(TEXT("FTD_SweepGazeOverBlankVhs: no player camera")); return false; }

		const FVector Center = Env->Bounds.Origin;
		const FVector Extent = Env->Bounds.BoxExtent;
		const FVector CamLoc = Player->GetFirstPersonCamera()->GetComponentLocation();
		const FVector ToBox = (Center - CamLoc).GetSafeNormal();

		const FVector Axes[3] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
		int32 Depth = 0; float Best = -1.f;
		for (int32 a = 0; a < 3; ++a)
		{
			const float D = FMath::Abs(FVector::DotProduct(ToBox, Axes[a]));
			if (D > Best) { Best = D; Depth = a; }
		}
		const int32 S1 = (Depth + 1) % 3, S2 = (Depth + 2) % 3;
		const FVector A = Axes[S1], B = Axes[S2];
		// Front face: push from the box center toward the camera along the depth axis.
		const float Sign = FMath::Sign(FVector::DotProduct(CamLoc - Center, Axes[Depth]));
		const FVector FaceCenter = Center + Axes[Depth] * (Extent[Depth] * Sign);

		// Sample at 85% of the half-extents so points stay on the visible face
		// (the AABB rim can sit just outside the mesh).
		const float Margin = 0.85f;
		const float EA = Extent[S1] * Margin, EB = Extent[S2] * Margin;
		for (int32 r = 0; r < GridN; ++r)
		{
			for (int32 c = 0; c < GridN; ++c)
			{
				const float u = FMath::Lerp(-EA, EA, (float)c / (GridN - 1));
				const float v = FMath::Lerp(-EB, EB, (float)r / (GridN - 1));
				Points.Add(FaceCenter + A * u + B * v);
			}
		}
		UE_LOG(LogTemp, Warning, TEXT("GazeSweep: %dx%d grid over blank VHS face center=%s extent(face)=(%.1f,%.1f) depthAxis=%d"),
			GridN, GridN, *FaceCenter.ToString(), EA, EB, Depth);
		return true;
	}

	int32 GridN;
	FString ShotPrefix;
	bool bInitialized = false;
	EPhase Phase = EPhase::Aim;
	int32 Index = 0;
	int32 SettleFrames = 0;
	int32 ShotFrames = 0;
	int32 LookingHits = 0;
	TArray<FVector> Points;
	TArray<int32> MissedCorners;
};

// =======================================================================
// FTD_TeleportFacingShelfBoxAndAim — stand directly in front of a shelf
// MovieBox (along its own forward vector, trying both sides), then aim at
// a TRACE-VERIFIED surface point probed from the player's real post-landing
// camera position. Derived bounds centers hit neighboring boxes at oblique
// angles — the blank-tape lesson, generalized to any labeled shelf box.
// =======================================================================

class FTD_TeleportFacingShelfBoxAndAim : public FTD_Base
{
public:
	FTD_TeleportFacingShelfBoxAndAim(FAutomationTestBase* InTest, FString InLabel, float InDistance = 80.f)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Distance(InDistance) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Standing in front of '%s' (verified aim)"), *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TeleportFacingShelfBoxAndAim: no driver")); return true; }
		AMovieBox* Box = Cast<AMovieBox>(Driver->FindActorByLabel(Label));
		if (!Box)
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportFacingShelfBoxAndAim: no MovieBox '%s'"), *Label));
			return true;
		}
		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_TeleportFacingShelfBoxAndAim: no player")); return true; }
		USceneComponent* Envelope = Driver->FindComponentOnActorByName(Box, TEXT("Cube"));
		if (!Envelope)
		{
			Test->AddError(FString::Printf(TEXT("FTD_TeleportFacingShelfBoxAndAim: '%s' has no 'Cube' envelope"), *Label));
			return true;
		}
		const FVector Center = Envelope->Bounds.Origin;

		if (bAwaitingProbe)
		{
			// Give the 5.7 floor snap a beat to settle, then probe from where
			// the camera ACTUALLY is — the exact trace the interact will run.
			if (FPlatformTime::Seconds() - TeleportTime < 0.3) { return false; }
			bAwaitingProbe = false;

			FCollisionObjectQueryParams ObjectParams;
			ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
			ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
			ObjectParams.AddObjectTypesToQuery(ECC_GameTraceChannel6);
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ShelfBoxAim), /*bTraceComplex*/ false);
			QueryParams.AddIgnoredActor(Player);

			const FVector Eye = Player->GetFirstPersonCamera()->GetComponentLocation();
			FHitResult Hit;
			const bool bHit = Box->GetWorld()->LineTraceSingleByObjectType(Hit, Eye, Center, ObjectParams, QueryParams);
			UE_LOG(LogTemp, Log, TEXT("FTD_TeleportFacingShelfBoxAndAim: '%s' side %d eye=%s -> hit %s at %s"),
				*Label, SideIndex, *Eye.ToString(),
				bHit && Hit.GetActor() ? *Hit.GetActor()->GetName() : TEXT("(nothing)"),
				bHit ? *Hit.ImpactPoint.ToString() : TEXT("-"));
			if (bHit && Hit.GetActor() == Box)
			{
				// Aim just inside the verified surface point.
				const FVector AimPoint = Hit.ImpactPoint + (Center - Eye).GetSafeNormal() * 3.f;
				if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
				{
					PC->SetControlRotation((AimPoint - Eye).Rotation());
				}
				return true;
			}
			SideIndex++;
		}

		if (SideIndex >= 2)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_TeleportFacingShelfBoxAndAim: neither side of '%s' has a clear interact trace"), *Label));
			return true;
		}

		FVector Forward = Box->GetActorForwardVector();
		Forward.Z = 0.f;
		Forward = Forward.GetSafeNormal();
		if (Forward.IsNearlyZero())
		{
			Forward = FVector::ForwardVector;
		}
		const float Sign = (SideIndex == 0) ? 1.f : -1.f;
		// Keep the player's current capsule height — same floor as the shelf
		// aisle; the floor snap settles any small error before the probe.
		const FVector TryLoc = FVector(Center.X, Center.Y, Player->GetActorLocation().Z) + Forward * (Distance * Sign);
		Player->SetActorLocation(TryLoc, false, nullptr, ETeleportType::TeleportPhysics);
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			PC->SetControlRotation((Center - TryLoc).Rotation());
		}
		TeleportTime = FPlatformTime::Seconds();
		bAwaitingProbe = true;
		return false;
	}
private:
	FString Label;
	float Distance;
	int32 SideIndex = 0;
	bool bAwaitingProbe = false;
	double TeleportTime = 0.0;
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
// FTD_SelectAndConfirmSlot — set cursor to slot N. Active item now follows
// navigation, so just moving the cursor assigns the item — no key press needed.
// =======================================================================

class FTD_SelectAndConfirmSlot : public FTD_Base
{
public:
	FTD_SelectAndConfirmSlot(FAutomationTestBase* InTest, int32 InIndex)
		: FTD_Base(InTest), Index(InIndex) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Selecting inventory slot %d"), Index);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (!Driver->SetSelectedSlot(Index))
		{
			Test->AddError(FString::Printf(TEXT("FTD_SelectAndConfirmSlot: failed to set slot %d"), Index));
		}
		return true;
	}
private:
	int32 Index;
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

		// Inject mouse rotation each frame so the player visibly rotates the
		// movie box (a few frames is plenty to exercise the rotation binding).
		Driver->SimulateMouseX(90.0f);
		FrameCount++;

		// After a few rotation frames, collect the movie directly via the
		// TestDriver — going through the E key here is fragile under UE 5.7
		// Enhanced Input because the rotation-phase legacy E presses poison
		// the player's interact-action DoOnce gate. The rotation step is the
		// thing under test; collecting via a direct API call still verifies
		// the inspection→collect→StopInspection flow.
		if (FrameCount >= 3 && !bCollectPressed)
		{
			Driver->TriggerCollectInspectedMovie();
			bCollectPressed = true;
		}

		return false;
	}
private:
	double Timeout;
	int32 FrameCount;
	bool bCollectPressed;
};

// =======================================================================
// FTD_WaitForInspectablePickupSpawned — poll the level until the key-break
// sequence has spawned its AInspectablePickup (the collectable broken key).
// =======================================================================

class FTD_WaitForInspectablePickupSpawned : public FTD_Base
{
public:
	FTD_WaitForInspectablePickupSpawned(FAutomationTestBase* InTest, double InTimeoutSeconds = 15.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override { return TEXT("Waiting for broken-key pickup to spawn"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		if (Driver->FindInspectablePickup())
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_WaitForInspectablePickupSpawned: no AInspectablePickup spawned before timeout"));
			return true;
		}
		return false;
	}
private:
	double Timeout;
};

// =======================================================================
// FTD_TeleportNearInspectablePickupAndAim — walk up to the spawned pickup
// and aim the camera at it so the next interact press hits it.
// =======================================================================

class FTD_TeleportNearInspectablePickupAndAim : public FTD_Base
{
public:
	FTD_TeleportNearInspectablePickupAndAim(FAutomationTestBase* InTest, float InDistance = 150.f)
		: FTD_Base(InTest), Distance(InDistance) {}

	virtual FString GetStatusText() const override { return TEXT("Teleporting to and aiming at broken-key pickup"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		AActor* Pickup = Driver->FindInspectablePickup();
		if (!Pickup)
		{
			Test->AddError(TEXT("FTD_TeleportNearInspectablePickupAndAim: no pickup found"));
			return true;
		}
		if (!Driver->TeleportNearActor(Pickup, Distance))
		{
			Test->AddError(TEXT("FTD_TeleportNearInspectablePickupAndAim: teleport failed"));
			return true;
		}
		if (!Driver->LookAt(Pickup))
		{
			Test->AddError(TEXT("FTD_TeleportNearInspectablePickupAndAim: LookAt failed"));
		}
		return true;
	}
private:
	float Distance;
};

// =======================================================================
// FTD_CollectInspectedPickup — once a pickup is in inspection, collect it
// directly via the TestDriver (the legacy "Collect Inspected Movie" binding
// is unreliable under simulated input in 5.7, same as movie collection),
// then wait for inspection to end. Mirrors FTD_RotateAndCollectMovie minus
// the rotation step.
// =======================================================================

class FTD_CollectInspectedPickup : public FTD_Base
{
public:
	FTD_CollectInspectedPickup(FAutomationTestBase* InTest, double InTimeoutSeconds = 5.0)
		: FTD_Base(InTest), Timeout(InTimeoutSeconds), FrameCount(0), bCollectTriggered(false) {}

	virtual FString GetStatusText() const override { return TEXT("Collecting inspected broken-key pickup"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		// CollectInspectedItem defers StopInspection one tick, which returns
		// the player to FreeRoaming — that's our completion signal.
		if (bCollectTriggered && Driver->GetActivityState() == EPlayerActivityState::FreeRoaming)
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(TEXT("FTD_CollectInspectedPickup: timed out"));
			return true;
		}

		// Let inspection settle a couple frames, then collect directly.
		FrameCount++;
		if (FrameCount >= 2 && !bCollectTriggered)
		{
			if (!Driver->TriggerCollectInspectedPickup())
			{
				Test->AddError(TEXT("FTD_CollectInspectedPickup: no pickup in inspection to collect"));
				return true;
			}
			bCollectTriggered = true;
		}
		return false;
	}
private:
	double Timeout;
	int32 FrameCount;
	bool bCollectTriggered;
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
		// NullRHI renders nothing, so the request is never serviced — and the
		// automation framework stops ticking latent commands while a screenshot
		// is pending, starving the whole queue forever (observed as multi-hour
		// hangs). Headless screenshots are blank anyway; skip the request.
		if (FParse::Param(FCommandLine::Get(), TEXT("nullrhi")))
		{
			UE_LOG(LogTemp, Log, TEXT("FTD_TakeScreenshot: '%s' skipped (NullRHI renders nothing)"), *Name);
			return true;
		}
		if (!bRequested)
		{
			FScreenshotRequest::RequestScreenshot(Name, false, false);
			bRequested = true;
			return false;
		}
		if (!FScreenshotRequest::IsScreenshotRequested())
		{
			return true;
		}
		// Watchdog for headed runs: a request the viewport never services must
		// not wedge the whole suite. Clear the stale request and move on.
		if (GetElapsedSinceFirstTick() > 10.0)
		{
			UE_LOG(LogTemp, Warning, TEXT("FTD_TakeScreenshot: '%s' never serviced after 10s — skipping"), *Name);
			FScreenshotRequest::Reset();
			return true;
		}
		return false;
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

// =======================================================================
// FTD_AddTestItem — inject an item directly into the inventory by mesh path,
// bypassing the gameplay flow that would normally grant it. For focused
// inventory tests that don't want to play through Seneca/Rick first.
// =======================================================================

class FTD_AddTestItem : public FTD_Base
{
public:
	FTD_AddTestItem(FAutomationTestBase* InTest, FName InItemId, FString InMeshPath, FVector InScale = FVector(1.f))
		: FTD_Base(InTest), ItemId(InItemId), MeshPath(MoveTemp(InMeshPath)), Scale(InScale) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Adding test item '%s'"), *ItemId.ToString());
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AddTestItem: no driver")); return true; }
		if (!Driver->AddTestItem(ItemId, MeshPath, Scale))
		{
			Test->AddError(FString::Printf(TEXT("FTD_AddTestItem: failed for '%s' / %s"),
				*ItemId.ToString(), *MeshPath));
		}
		return true;
	}
private:
	FName ItemId;
	FString MeshPath;
	FVector Scale;
};

// =======================================================================
// FTD_AddAllItemDefsFromFolder — load every UItemDefinition under a content
// path (e.g. "/Game/Inventory") and grant each to the player. For tour-style
// tests that want to visualize every item without playing through gameplay.
// =======================================================================

class FTD_AddAllItemDefsFromFolder : public FTD_Base
{
public:
	FTD_AddAllItemDefsFromFolder(FAutomationTestBase* InTest, FString InFolderPath, int32 InExpectedMin = 1)
		: FTD_Base(InTest), FolderPath(MoveTemp(InFolderPath)), ExpectedMin(InExpectedMin) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Adding all item defs from %s"), *FolderPath);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AddAllItemDefsFromFolder: no driver")); return true; }
		const int32 Added = Driver->AddAllItemDefsFromFolder(FolderPath);
		if (Added < ExpectedMin)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AddAllItemDefsFromFolder: only %d added from %s (expected >= %d)"),
				Added, *FolderPath, ExpectedMin));
		}
		return true;
	}
private:
	FString FolderPath;
	int32 ExpectedMin;
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
// FTD_SetStoryFlag / FTD_AssertStoryFlag — drive and read the central
// UStorySubsystem flags by name through the TestDriver.
// =======================================================================

class FTD_SetStoryFlag : public FTD_Base
{
public:
	FTD_SetStoryFlag(FAutomationTestBase* InTest, FName InFlag, bool InValue)
		: FTD_Base(InTest), Flag(InFlag), Value(InValue) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Setting story flag '%s' = %s"), *Flag.ToString(), Value ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SetStoryFlag: no driver")); return true; }
		Driver->SetStoryFlag(Flag, Value);
		return true;
	}
private:
	FName Flag;
	bool Value;
};

class FTD_AssertStoryFlag : public FTD_Base
{
public:
	FTD_AssertStoryFlag(FAutomationTestBase* InTest, FName InFlag, bool InExpected)
		: FTD_Base(InTest), Flag(InFlag), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting story flag '%s' == %s"), *Flag.ToString(), Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertStoryFlag: no driver")); return true; }
		const bool Actual = Driver->IsStoryFlagSet(Flag);
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertStoryFlag: '%s' is %s, expected %s"),
				*Flag.ToString(), Actual ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	FName Flag;
	bool Expected;
};

// Poll a story flag until it reaches the expected value (for gaze-driven flags
// that flip after a dwell).
class FTD_WaitForStoryFlag : public FTD_Base
{
public:
	FTD_WaitForStoryFlag(FAutomationTestBase* InTest, FName InFlag, bool InExpected, double InTimeout = 8.0)
		: FTD_Base(InTest), Flag(InFlag), Expected(InExpected), Timeout(InTimeout) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for story flag '%s' == %s"), *Flag.ToString(), Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_WaitForStoryFlag: no driver")); return true; }
		if (Driver->IsStoryFlagSet(Flag) == Expected)
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForStoryFlag: '%s' never reached %s within %.0fs"),
				*Flag.ToString(), Expected ? TEXT("true") : TEXT("false"), Timeout));
			return true;
		}
		return false;
	}
private:
	FName Flag;
	bool Expected;
	double Timeout;
};

// Invoke the store-entry handler directly (no physical walk through the trigger).
class FTD_TriggerStoreEntry : public FTD_Base
{
public:
	FTD_TriggerStoreEntry(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Triggering store entry"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TriggerStoreEntry: no driver")); return true; }
		Driver->TriggerStoreEntry();
		return true;
	}
};

// Assert an ACRTTV (by label) is / isn't showing its tornado-warning screen.
class FTD_AssertTvShowingWarning : public FTD_Base
{
public:
	FTD_AssertTvShowingWarning(FAutomationTestBase* InTest, FString InLabel, bool InExpected)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting TV '%s' showing warning == %s"), *Label, Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertTvShowingWarning: no driver")); return true; }
		const bool Actual = Driver->IsTvShowingWarning(Label);
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertTvShowingWarning: '%s' showing=%s, expected %s"),
				*Label, Actual ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	FString Label;
	bool Expected;
};

// Assert an actor's root visibility (for the gated telephone scene).
class FTD_AssertActorVisible : public FTD_Base
{
public:
	FTD_AssertActorVisible(FAutomationTestBase* InTest, FString InLabel, bool InExpected)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting '%s' visible == %s"), *Label, Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertActorVisible: no driver")); return true; }
		const bool Actual = Driver->IsActorVisibleByLabel(Label);
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertActorVisible: '%s' visible=%s, expected %s"),
				*Label, Actual ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	FString Label;
	bool Expected;
};

// Trigger the pay-phone pickup directly.
class FTD_TriggerPayPhonePickup : public FTD_Base
{
public:
	FTD_TriggerPayPhonePickup(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Triggering pay-phone pickup"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_TriggerPayPhonePickup: no driver")); return true; }
		Driver->TriggerPayPhonePickup();
		return true;
	}
};

// Assert whether the pay-phone audio bed is playing.
class FTD_AssertPayPhoneAudioPlaying : public FTD_Base
{
public:
	FTD_AssertPayPhoneAudioPlaying(FAutomationTestBase* InTest, bool InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting pay-phone audio playing == %s"), Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertPayPhoneAudioPlaying: no driver")); return true; }
		const bool Actual = Driver->IsPayPhoneAudioPlaying();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertPayPhoneAudioPlaying: playing=%s, expected %s"),
				Actual ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	bool Expected;
};

// Assert whether the pay-phone would accept an interact (gated + one-shot).
class FTD_AssertPayPhoneCanInteract : public FTD_Base
{
public:
	FTD_AssertPayPhoneCanInteract(FAutomationTestBase* InTest, bool InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting pay-phone can-interact == %s"), Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertPayPhoneCanInteract: no driver")); return true; }
		const bool Actual = Driver->CanPayPhoneInteract();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertPayPhoneCanInteract: canInteract=%s, expected %s"),
				Actual ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	bool Expected;
};

// =======================================================================
// FTD_AssertSenecaSmokingLines — the joined Smoking-state lines must (or
// must not) contain a substring. Drives the shelter-line gating check.
// =======================================================================

class FTD_AssertSenecaSmokingLines : public FTD_Base
{
public:
	FTD_AssertSenecaSmokingLines(FAutomationTestBase* InTest, FString InNeedle, bool bInShouldContain)
		: FTD_Base(InTest), Needle(MoveTemp(InNeedle)), bShouldContain(bInShouldContain) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting Seneca smoking lines %s '%s'"),
			bShouldContain ? TEXT("contain") : TEXT("omit"), *Needle);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertSenecaSmokingLines: no driver")); return true; }
		FString Joined;
		if (!Driver->GetSenecaSmokingLinesJoined(Joined))
		{
			Test->AddError(TEXT("FTD_AssertSenecaSmokingLines: no Seneca"));
			return true;
		}
		const bool bContains = Joined.Contains(Needle, ESearchCase::IgnoreCase);
		if (bContains != bShouldContain)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertSenecaSmokingLines: lines %s '%s' but expected %s. Lines: \"%s\""),
				bContains ? TEXT("contain") : TEXT("omit"), *Needle,
				bShouldContain ? TEXT("contain") : TEXT("omit"), *Joined));
		}
		return true;
	}
private:
	FString Needle;
	bool bShouldContain;
};

// =======================================================================
// FTD_ForceSenecaSmoking — jump Seneca into the Smoking beat directly.
// =======================================================================

class FTD_ForceSenecaSmoking : public FTD_Base
{
public:
	FTD_ForceSenecaSmoking(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Forcing Seneca into the smoking beat"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_ForceSenecaSmoking: no driver")); return true; }
		Driver->ForceSenecaToSmoking();
		return true;
	}
};

// =======================================================================
// FTD_AdvanceDialogueUntilLineContains — press E until the displayed
// dialogue body contains a substring (case-insensitive), then stop.
// =======================================================================

class FTD_AdvanceDialogueUntilLineContains : public FTD_Base
{
public:
	FTD_AdvanceDialogueUntilLineContains(FAutomationTestBase* InTest, FString InNeedle,
		double InLineDelay = 1.0, double InTimeoutSeconds = 20.0)
		: FTD_Base(InTest), Needle(MoveTemp(InNeedle)), LineDelay(InLineDelay), Timeout(InTimeoutSeconds)
		, LastPressTime(0.0), bWaitingForRelease(false) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Advancing dialogue until line contains '%s'"), *Needle);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("no driver")); return true; }

		const EPlayerActivityState State = Driver->GetActivityState();
		const bool bInDialogue =
			State == EPlayerActivityState::InSimpleDialogue ||
			State == EPlayerActivityState::InDialogue;

		// Only read the widget while dialogue is actually up — otherwise the
		// query spams "no active dialogue widget" every frame.
		if (bInDialogue)
		{
			FString Speaker, Body;
			if (Driver->GetDisplayedDialogue(Speaker, Body) && Body.Contains(Needle, ESearchCase::IgnoreCase))
			{
				if (bWaitingForRelease) { Driver->SimulateInteractRelease(); bWaitingForRelease = false; }
				LastBody = Body;
				return true;
			}
			LastBody = Body;
		}

		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AdvanceDialogueUntilLineContains: '%s' never shown (last body: \"%s\")"), *Needle, *LastBody));
			return true;
		}

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
	FString Needle;
	double LineDelay;
	double Timeout;
	double LastPressTime;
	bool bWaitingForRelease;
	FString LastBody;
};

// FTD_AssertGazeRewardSeconds — read the UGazeRewardComponent's dwell timer
// and assert it's within [Min, Max]. For the GazeRewardReset test.
class FTD_AssertGazeRewardSeconds : public FTD_Base
{
public:
	FTD_AssertGazeRewardSeconds(FAutomationTestBase* InTest, FString InLabel, float InMin, float InMax)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Min(InMin), Max(InMax) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting gaze-reward seconds in [%.2f, %.2f] (%s)"), Min, Max, *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertGazeRewardSeconds: no driver")); return true; }
		float Seconds = -1.f;
		if (!Driver->GetGazeRewardSeconds(Seconds))
		{
			Test->AddError(TEXT("FTD_AssertGazeRewardSeconds: no UGazeRewardComponent in level"));
			return true;
		}
		UE_LOG(LogTemp, Log, TEXT("FTD_AssertGazeRewardSeconds [%s]: seconds=%.2f (expect [%.2f, %.2f])"),
			*Label, Seconds, Min, Max);
		if (Seconds < Min || Seconds > Max)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertGazeRewardSeconds [%s]: seconds=%.2f outside [%.2f, %.2f]"),
				*Label, Seconds, Min, Max));
		}
		return true;
	}
private:
	FString Label;
	float Min;
	float Max;
};

// FTD_WaitForGazeSeconds — poll the gaze dwell timer until it reaches Min.
// The player teleports above the lot and falls for ~1s+; the gaze only holds
// continuously once they've landed, so wait rather than assume a fixed delay.
class FTD_WaitForGazeSeconds : public FTD_Base
{
public:
	FTD_WaitForGazeSeconds(FAutomationTestBase* InTest, float InMinSeconds, double InTimeout = 12.0)
		: FTD_Base(InTest), MinSeconds(InMinSeconds), Timeout(InTimeout) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for gaze to reach %.1fs"), MinSeconds);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_WaitForGazeSeconds: no driver")); return true; }
		float Seconds = 0.f;
		Driver->GetGazeRewardSeconds(Seconds);
		if (Seconds >= MinSeconds) { return true; }
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForGazeSeconds: gaze only reached %.2fs of %.1fs within %.0fs"),
				Seconds, MinSeconds, Timeout));
			return true;
		}
		return false;
	}
private:
	float MinSeconds;
	double Timeout;
};

// FTD_AssertGazeEffectWeight — read the UGazeRewardComponent's screen-effect
// blendable weight and assert it's within [Min, Max].
class FTD_AssertGazeEffectWeight : public FTD_Base
{
public:
	FTD_AssertGazeEffectWeight(FAutomationTestBase* InTest, FString InLabel, float InMin, float InMax)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Min(InMin), Max(InMax) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting gaze effect weight in [%.2f, %.2f] (%s)"), Min, Max, *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertGazeEffectWeight: no driver")); return true; }
		float W = -1.f;
		if (!Driver->GetGazeEffectWeight(W))
		{
			Test->AddError(TEXT("FTD_AssertGazeEffectWeight: no UGazeRewardComponent in level"));
			return true;
		}
		UE_LOG(LogTemp, Log, TEXT("FTD_AssertGazeEffectWeight [%s]: weight=%.3f (expect [%.2f, %.2f])"),
			*Label, W, Min, Max);
		if (W < Min || W > Max)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertGazeEffectWeight [%s]: weight=%.3f outside [%.2f, %.2f]"),
				*Label, W, Min, Max));
		}
		return true;
	}
private:
	FString Label;
	float Min;
	float Max;
};

// FTD_AssertGazeCameraFOV — read the player camera's FOV (driven by the gaze
// FOV-zoom) and assert it's within [Min, Max].
class FTD_AssertGazeCameraFOV : public FTD_Base
{
public:
	FTD_AssertGazeCameraFOV(FAutomationTestBase* InTest, FString InLabel, float InMin, float InMax)
		: FTD_Base(InTest), Label(MoveTemp(InLabel)), Min(InMin), Max(InMax) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting gaze camera FOV in [%.1f, %.1f] (%s)"), Min, Max, *Label);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertGazeCameraFOV: no driver")); return true; }
		float FOV = -1.f;
		if (!Driver->GetGazeCameraFOV(FOV))
		{
			Test->AddError(TEXT("FTD_AssertGazeCameraFOV: no gaze component"));
			return true;
		}
		UE_LOG(LogTemp, Log, TEXT("FTD_AssertGazeCameraFOV [%s]: fov=%.2f (expect [%.1f, %.1f])"), *Label, FOV, Min, Max);
		if (FOV < Min || FOV > Max)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertGazeCameraFOV [%s]: fov=%.2f outside [%.1f, %.1f]"),
				*Label, FOV, Min, Max));
		}
		return true;
	}
private:
	FString Label;
	float Min;
	float Max;
};

// FTD_LookDown — aim the camera at the ground to break gaze on an overhead
// fixture (the canopy light is far above the player).
class FTD_LookDown : public FTD_Base
{
public:
	FTD_LookDown(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Looking down (break gaze)"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_LookDown: no driver")); return true; }
		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_LookDown: no player")); return true; }
		if (APlayerController* PC = Cast<APlayerController>(Player->GetController()))
		{
			PC->SetControlRotation(FRotator(-80.f, 0.f, 0.f));
		}
		return true;
	}
};

// =======================================================================
// FTD_WaitForItemAdded — poll HasItem(Id) until it returns true.
// Useful for waiting on async item grants (e.g. collecting the broken-key
// pickup the key-break sequence drops on the ground).
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
// FTD_AssertGazeHumRising — sample the gaze-reward hum volume at two
// times into a held stare and assert it's playing and getting louder.
// Latent commands tick sequentially, so this command owns the stare
// timeline between SampleT1 and SampleT2 while the camera holds its aim.
// =======================================================================

class FTD_AssertGazeHumRising : public FTD_Base
{
public:
	FTD_AssertGazeHumRising(FAutomationTestBase* InTest, double InSampleT1, double InSampleT2)
		: FTD_Base(InTest), SampleT1(InSampleT1), SampleT2(InSampleT2) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting gaze hum rises between %.0fs and %.0fs"), SampleT1, SampleT2);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertGazeHumRising: no driver")); return true; }

		const double Elapsed = GetElapsedSinceFirstTick();
		if (!bSampledV1)
		{
			if (Elapsed < SampleT1) { return false; }
			bool bPlaying = false;
			if (!Driver->GetGazeHumState(V1, bPlaying))
			{
				Test->AddError(TEXT("FTD_AssertGazeHumRising: no gaze-reward hum in level"));
				return true;
			}
			if (!bPlaying || V1 <= 0.f)
			{
				Test->AddError(FString::Printf(TEXT("FTD_AssertGazeHumRising: hum not audible at %.1fs (playing=%d vol=%.3f)"),
					Elapsed, bPlaying ? 1 : 0, V1));
				return true;
			}
			bSampledV1 = true;
			return false;
		}

		if (Elapsed < SampleT2) { return false; }
		float V2 = 0.f;
		bool bPlaying = false;
		if (!Driver->GetGazeHumState(V2, bPlaying) || !bPlaying || V2 <= V1)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertGazeHumRising: hum not rising (V1=%.3f V2=%.3f playing=%d)"),
				V1, V2, bPlaying ? 1 : 0));
		}
		return true;
	}
private:
	double SampleT1;
	double SampleT2;
	float V1 = 0.f;
	bool bSampledV1 = false;
};

// =======================================================================
// FTD_AssertDialogueLine — the dialogue widget must currently show exactly
// this speaker plate and body text. Exact body match catches speaker
// prefixes leaking into the displayed line.
// =======================================================================

class FTD_AssertDialogueLine : public FTD_Base
{
public:
	FTD_AssertDialogueLine(FAutomationTestBase* InTest, FString InExpectedSpeaker, FString InExpectedBody)
		: FTD_Base(InTest), ExpectedSpeaker(MoveTemp(InExpectedSpeaker)), ExpectedBody(MoveTemp(InExpectedBody)) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting dialogue shows [%s] \"%s\""), *ExpectedSpeaker, *ExpectedBody);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertDialogueLine: no driver")); return true; }

		FString Speaker, Body;
		if (!Driver->GetDisplayedDialogue(Speaker, Body))
		{
			Test->AddError(TEXT("FTD_AssertDialogueLine: no dialogue is being displayed"));
			return true;
		}
		if (Speaker != ExpectedSpeaker)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertDialogueLine: speaker plate is \"%s\", expected \"%s\""),
				*Speaker, *ExpectedSpeaker));
		}
		if (Body != ExpectedBody)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertDialogueLine: body is \"%s\", expected \"%s\""),
				*Body, *ExpectedBody));
		}
		return true;
	}
private:
	FString ExpectedSpeaker;
	FString ExpectedBody;
};

// =======================================================================
// FTD_AssertPutBackPrompt — while a movie is inspected, a world-space
// prompt must be visible, name the put-back binding, and face the camera.
// =======================================================================

class FTD_AssertPutBackPrompt : public FTD_Base
{
public:
	FTD_AssertPutBackPrompt(FAutomationTestBase* InTest, FString InExpectedText, float InMinFacingDot = 0.94f)
		: FTD_Base(InTest), ExpectedText(MoveTemp(InExpectedText)), MinFacingDot(InMinFacingDot) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting put-back prompt reads \"%s\" and faces camera"), *ExpectedText);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertPutBackPrompt: no driver")); return true; }

		FString Text;
		float FacingDot = 0.f;
		bool bVisible = false;
		if (!Driver->GetPutBackPromptState(Text, FacingDot, bVisible))
		{
			Test->AddError(TEXT("FTD_AssertPutBackPrompt: no inspected MovieBox with a PutBackPrompt"));
			return true;
		}
		if (!bVisible)
		{
			Test->AddError(TEXT("FTD_AssertPutBackPrompt: prompt exists but is not visible"));
		}
		// Exact match: catches both a wrong binding and the dual "Q / B" form
		// when only the active device's key should show.
		if (Text != ExpectedText)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertPutBackPrompt: text is \"%s\", expected \"%s\""),
				*Text, *ExpectedText));
		}
		if (FacingDot < MinFacingDot)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertPutBackPrompt: not facing camera (dot=%.3f, need >=%.2f)"),
				FacingDot, MinFacingDot));
		}
		return true;
	}
private:
	FString ExpectedText;
	float MinFacingDot;
};

// =======================================================================
// FTD_ActivateBlankTape — test bypass: trigger the spawner's chosen-tape
// swap directly instead of playing through the money beat.
// =======================================================================

class FTD_ActivateBlankTape : public FTD_Base
{
public:
	FTD_ActivateBlankTape(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Activating blank tape (test bypass)"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_ActivateBlankTape: no driver")); return true; }
		if (!Driver->ActivateBlankTapeForTest())
		{
			Test->AddError(TEXT("FTD_ActivateBlankTape: failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_CollectBlankTape — test bypass: collect the blank tape through the
// production capture path without shelf aiming (HappyPath covers aiming).
// =======================================================================

class FTD_CollectBlankTape : public FTD_Base
{
public:
	FTD_CollectBlankTape(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Collecting blank tape (test bypass)"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_CollectBlankTape: no driver")); return true; }
		if (!Driver->CollectBlankTapeForTest())
		{
			Test->AddError(TEXT("FTD_CollectBlankTape: failed"));
		}
		return true;
	}
};

// =======================================================================
// FTD_CaptureHeldBoxAxes / FTD_AssertHeldBoxAxesMatch — record the held
// item's camera-space long/short box axes into caller-owned vectors, then
// later assert the currently held item's axes match (same "pose" for two
// box-shaped items regardless of mesh authoring). Abs dots: a box flipped
// 180° reads as the same held pose.
// =======================================================================

class FTD_CaptureHeldBoxAxes : public FTD_Base
{
public:
	FTD_CaptureHeldBoxAxes(FAutomationTestBase* InTest, FVector* InOutLong, FVector* InOutShort, float* InOutMaxExtent, FVector* InOutCenter)
		: FTD_Base(InTest), OutLong(InOutLong), OutShort(InOutShort), OutMaxExtent(InOutMaxExtent), OutCenter(InOutCenter) {}

	virtual FString GetStatusText() const override { return TEXT("Capturing held item box axes"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_CaptureHeldBoxAxes: no driver")); return true; }
		if (!Driver->GetHeldItemBoxAxes(*OutLong, *OutShort, *OutMaxExtent, *OutCenter))
		{
			Test->AddError(TEXT("FTD_CaptureHeldBoxAxes: no visible held item"));
		}
		return true;
	}
private:
	FVector* OutLong;
	FVector* OutShort;
	float* OutMaxExtent;
	FVector* OutCenter;
};

class FTD_AssertHeldBoxAxesMatch : public FTD_Base
{
public:
	FTD_AssertHeldBoxAxesMatch(FAutomationTestBase* InTest, const FVector* InLong, const FVector* InShort,
		const float* InMaxExtent, const FVector* InCenter,
		float InMinLongDot = 0.95f, float InMinShortDot = 0.90f, float InMaxSizeRatio = 2.0f, float InMaxCenterDelta = 15.f)
		: FTD_Base(InTest), RefLong(InLong), RefShort(InShort), RefMaxExtent(InMaxExtent), RefCenter(InCenter)
		, MinLongDot(InMinLongDot), MinShortDot(InMinShortDot), MaxSizeRatio(InMaxSizeRatio), MaxCenterDelta(InMaxCenterDelta) {}

	virtual FString GetStatusText() const override { return TEXT("Asserting held item pose matches captured axes"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertHeldBoxAxesMatch: no driver")); return true; }

		FVector Long, Short, Center;
		float MaxExtent = 0.f;
		if (!Driver->GetHeldItemBoxAxes(Long, Short, MaxExtent, Center))
		{
			Test->AddError(TEXT("FTD_AssertHeldBoxAxesMatch: no visible held item"));
			return true;
		}

		const float LongDot = FMath::Abs(FVector::DotProduct(Long, *RefLong));
		const float ShortDot = FMath::Abs(FVector::DotProduct(Short, *RefShort));
		if (LongDot < MinLongDot || ShortDot < MinShortDot)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertHeldBoxAxesMatch: pose differs (longDot=%.3f need>=%.2f, shortDot=%.3f need>=%.2f; held long=%s ref long=%s)"),
				LongDot, MinLongDot, ShortDot, MinShortDot, *Long.ToString(), *RefLong->ToString()));
		}

		const float SizeRatio = (*RefMaxExtent > KINDA_SMALL_NUMBER) ? (MaxExtent / *RefMaxExtent) : 0.f;
		if (SizeRatio < 1.f / MaxSizeRatio || SizeRatio > MaxSizeRatio)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertHeldBoxAxesMatch: size differs (held extent %.1fcm vs ref %.1fcm, ratio %.2f outside 1/%.1f..%.1f)"),
				MaxExtent, *RefMaxExtent, SizeRatio, MaxSizeRatio, MaxSizeRatio));
		}

		const float CenterDelta = FVector::Dist(Center, *RefCenter);
		if (CenterDelta > MaxCenterDelta)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertHeldBoxAxesMatch: held position differs (center %s vs ref %s, delta %.1fcm > %.1fcm)"),
				*Center.ToString(), *RefCenter->ToString(), CenterDelta, MaxCenterDelta));
		}
		return true;
	}
private:
	const FVector* RefLong;
	const FVector* RefShort;
	const float* RefMaxExtent;
	const FVector* RefCenter;
	float MinLongDot;
	float MinShortDot;
	float MaxSizeRatio;
	float MaxCenterDelta;
};

// =======================================================================
// FTD_AssertGazeHumStopped — the hum must not keep playing once the
// gaze reward has fired.
// =======================================================================

class FTD_AssertGazeHumStopped : public FTD_Base
{
public:
	FTD_AssertGazeHumStopped(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Asserting gaze hum stopped"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertGazeHumStopped: no driver")); return true; }
		float Volume = 0.f;
		bool bPlaying = false;
		if (!Driver->GetGazeHumState(Volume, bPlaying))
		{
			Test->AddError(TEXT("FTD_AssertGazeHumStopped: no gaze-reward hum in level"));
			return true;
		}
		if (bPlaying)
		{
			Test->AddError(TEXT("FTD_AssertGazeHumStopped: hum still playing after reward"));
		}
		return true;
	}
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
// FTD_SetMouseLookSensitivity — write directly to the persisted settings
// (clamped + snapped). Used by sensitivity diagnostic tests that drive the
// mouse look path via FTD_InjectMouseXForDuration.
// =======================================================================

class FTD_SetMouseLookSensitivity : public FTD_Base
{
public:
	FTD_SetMouseLookSensitivity(FAutomationTestBase* InTest, float InValue)
		: FTD_Base(InTest), Value(InValue) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Setting mouse look sensitivity to %.3f"), Value);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SetMouseLookSensitivity: no driver")); return true; }
		Driver->SetMouseLookSensitivity(Value);
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

// =======================================================================
// FTD_SimulateSettingsPress — inject IA_Settings via Enhanced Input.
// Press → 1 frame gap → Release (mirrors FTD_SimulateInventoryAction).
// =======================================================================

class FTD_SimulateSettingsPress : public FTD_Base
{
public:
	FTD_SimulateSettingsPress(FAutomationTestBase* InTest) : FTD_Base(InTest), bPressed(false) {}

	virtual FString GetStatusText() const override { return TEXT("Pressing settings key"); }

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SimulateSettingsPress: no driver")); return true; }

		if (!bPressed)
		{
			Driver->SimulateSettingsPress();
			bPressed = true;
			return false;
		}
		Driver->SimulateSettingsRelease();
		return true;
	}
private:
	bool bPressed;
};

// =======================================================================
// Nav action selector for FTD_SimulateNavAction. Resolves to a UInputAction*
// at tick time via the player accessors.
// =======================================================================
enum class ENavInputAction : uint8
{
	NextOption,
	PreviousOption,
	NavigateLeft,
	NavigateRight,
};

// =======================================================================
// FTD_SimulateNavAction — inject one of the four menu nav actions through
// Enhanced Input. Press → 1 frame gap → Release so the consumer's Started
// trigger event fires exactly once.
// =======================================================================

class FTD_SimulateNavAction : public FTD_Base
{
public:
	FTD_SimulateNavAction(FAutomationTestBase* InTest, ENavInputAction InAction)
		: FTD_Base(InTest), NavAction(InAction), bPressed(false) {}

	virtual FString GetStatusText() const override
	{
		const TCHAR* Name = TEXT("?");
		switch (NavAction)
		{
		case ENavInputAction::NextOption:     Name = TEXT("NextOption"); break;
		case ENavInputAction::PreviousOption: Name = TEXT("PreviousOption"); break;
		case ENavInputAction::NavigateLeft:   Name = TEXT("NavigateLeft"); break;
		case ENavInputAction::NavigateRight:  Name = TEXT("NavigateRight"); break;
		}
		return FString::Printf(TEXT("Injecting IA_%s"), Name);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_SimulateNavAction: no driver")); return true; }

		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_SimulateNavAction: no player")); return true; }

		UInputAction* Action = nullptr;
		switch (NavAction)
		{
		case ENavInputAction::NextOption:     Action = Player->GetNextOptionAction(); break;
		case ENavInputAction::PreviousOption: Action = Player->GetPreviousOptionAction(); break;
		case ENavInputAction::NavigateLeft:   Action = Player->GetNavigateLeftAction(); break;
		case ENavInputAction::NavigateRight:  Action = Player->GetNavigateRightAction(); break;
		}
		if (!Action) { Test->AddError(TEXT("FTD_SimulateNavAction: action accessor returned null")); return true; }

		if (!bPressed)
		{
			Driver->InjectInputAction(Action, true);
			bPressed = true;
			return false;
		}
		Driver->InjectInputAction(Action, false);
		return true;
	}
private:
	ENavInputAction NavAction;
	bool bPressed;
};

// =======================================================================
// FTD_AssertMenuPage — verify the menu actor's current page.
// =======================================================================

class FTD_AssertMenuPage : public FTD_Base
{
public:
	FTD_AssertMenuPage(FAutomationTestBase* InTest, EMenuPage InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting menu page == %d"), (int32)Expected);
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertMenuPage: no driver")); return true; }

		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_AssertMenuPage: no player")); return true; }

		UMenuUIComponent* Menu = Player->GetMenuUIComponent();
		if (!Menu) { Test->AddError(TEXT("FTD_AssertMenuPage: no MenuUIComponent")); return true; }

		AMenuUIActor* Actor = Menu->GetMenuActor();
		if (!Actor) { Test->AddError(TEXT("FTD_AssertMenuPage: no MenuUIActor")); return true; }

		const EMenuPage Actual = Actor->GetCurrentPage();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertMenuPage: expected %d but got %d"),
				(int32)Expected, (int32)Actual));
		}
		return true;
	}
private:
	EMenuPage Expected;
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

// =======================================================================
// FTD_AssertInventoryFlashlight — verify the player's RectLight (used by
// both inventory and pause menu) is enabled/disabled.
// =======================================================================

class FTD_AssertInventoryFlashlight : public FTD_Base
{
public:
	FTD_AssertInventoryFlashlight(FAutomationTestBase* InTest, bool InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting inventory flashlight == %s"), Expected ? TEXT("ON") : TEXT("OFF"));
	}

	virtual bool UpdateStep() override
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (!Driver) { Test->AddError(TEXT("FTD_AssertInventoryFlashlight: no driver")); return true; }

		AFirstPersonCharacter* Player = Driver->GetPlayer();
		if (!Player) { Test->AddError(TEXT("FTD_AssertInventoryFlashlight: no player")); return true; }

		const bool Actual = Player->IsInventoryFlashlightEnabled();
		if (Actual != Expected)
		{
			Test->AddError(FString::Printf(
				TEXT("FTD_AssertInventoryFlashlight: expected %s but got %s"),
				Expected ? TEXT("ON") : TEXT("OFF"),
				Actual   ? TEXT("ON") : TEXT("OFF")));
		}
		return true;
	}
private:
	bool Expected;
};

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
