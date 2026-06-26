#include "E2E_Steps.h" // force rebuild

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "UltraDynamicWeatherController.h"
#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "Sound/AmbientSound.h"
#include "UObject/UnrealType.h"

// =======================================================================
// Storm-sky helpers + latent commands — drive and read the post-KeyBroke
// Overall-Intensity fade on AUltraDynamicWeatherController / Ultra_Dynamic_Sky.
// =======================================================================

namespace StormSkyTest
{
	inline AUltraDynamicWeatherController* FindController(UWorld* World)
	{
		if (!World) { return nullptr; }
		for (TActorIterator<AUltraDynamicWeatherController> It(World); It; ++It)
		{
			return *It;
		}
		return nullptr;
	}

	// Read a floating-point property (literal-space FName) off the first actor whose
	// class name contains Needle, via the same FNumericProperty path the controller
	// writes. False if no matching actor / no property.
	inline bool ReadActorFloat(UWorld* World, const TCHAR* Needle, const TCHAR* PropName, float& OutValue)
	{
		if (!World) { return false; }
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->GetClass()->GetName().Contains(Needle))
			{
				continue;
			}
			FNumericProperty* Num = CastField<FNumericProperty>(It->GetClass()->FindPropertyByName(FName(PropName)));
			if (Num && Num->IsFloatingPoint())
			{
				OutValue = static_cast<float>(Num->GetFloatingPointPropertyValue(Num->ContainerPtrToValuePtr<void>(*It)));
				return true;
			}
		}
		return false;
	}

	inline bool ReadSkyIntensity(UWorld* World, float& OutValue)
	{
		return ReadActorFloat(World, TEXT("Ultra_Dynamic_Sky"), TEXT("Overall Intensity"), OutValue);
	}

	inline bool ReadWeatherWind(UWorld* World, float& OutValue)
	{
		return ReadActorFloat(World, TEXT("Ultra_Dynamic_Weather"), TEXT("Wind Intensity"), OutValue);
	}

	// Find the placed "Ambient_GlobalWind" AmbientSound by editor label (available in
	// the editor-context E2E even though it isn't at shipping runtime).
	inline AAmbientSound* FindAmbientGlobalWind(UWorld* World)
	{
		if (!World) { return nullptr; }
		for (TActorIterator<AAmbientSound> It(World); It; ++It)
		{
			if (It->GetActorLabel() == TEXT("Ambient_GlobalWind"))
			{
				return *It;
			}
		}
		return nullptr;
	}

	inline bool ReadAmbientWindVolume(UWorld* World, float& OutValue)
	{
		AAmbientSound* Wind = FindAmbientGlobalWind(World);
		UAudioComponent* AudioComp = Wind ? Wind->GetAudioComponent() : nullptr;
		if (!AudioComp) { return false; }
		OutValue = AudioComp->VolumeMultiplier;
		return true;
	}

	// Read the weather actor's "Wind Intensity - Manual Override" bool.
	inline bool ReadWeatherWindOverride(UWorld* World, bool& OutValue)
	{
		if (!World) { return false; }
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Weather")))
			{
				continue;
			}
			FBoolProperty* B = CastField<FBoolProperty>(It->GetClass()->FindPropertyByName(FName(TEXT("Wind Intensity - Manual Override"))));
			if (B)
			{
				OutValue = B->GetPropertyValue_InContainer(*It);
				return true;
			}
		}
		return false;
	}
}

// Shorten the fade and pin the target so the test doesn't sit through the full
// 2-minute default. Runs before KeyBroke so BeginFade picks up the new duration.
class FTD_ConfigureStormSkyFade : public FTD_Base
{
public:
	FTD_ConfigureStormSkyFade(FAutomationTestBase* InTest, float InDuration, float InTargetIntensity, float InStartWind, float InTargetWind, float InTargetVolume)
		: FTD_Base(InTest), Duration(InDuration), TargetIntensity(InTargetIntensity), StartWind(InStartWind), TargetWind(InTargetWind), TargetVolume(InTargetVolume) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Configuring storm transition: %.2fs, intensity -> %.2f, wind %.1f -> %.1f, volume -> %.2f"), Duration, TargetIntensity, StartWind, TargetWind, TargetVolume);
	}

	virtual bool UpdateStep() override
	{
		AUltraDynamicWeatherController* Ctrl = StormSkyTest::FindController(E2ELatent::GetPIEWorld());
		if (!Ctrl)
		{
			Test->AddError(TEXT("FTD_ConfigureStormSkyFade: no AUltraDynamicWeatherController in the level"));
			return true;
		}
		Ctrl->FadeDuration = Duration;
		Ctrl->TargetOverallIntensity = TargetIntensity;
		Ctrl->StartWindIntensity = StartWind;
		Ctrl->TargetWindIntensity = TargetWind;
		Ctrl->TargetWindVolume = TargetVolume;
		// Wire the wind ambient (designer-assigned in real play; found by label here).
		Ctrl->AmbientGlobalWind = StormSkyTest::FindAmbientGlobalWind(E2ELatent::GetPIEWorld());
		if (!Ctrl->AmbientGlobalWind)
		{
			Test->AddError(TEXT("FTD_ConfigureStormSkyFade: no 'Ambient_GlobalWind' AmbientSound in the level"));
		}
		return true;
	}
private:
	float Duration;
	float TargetIntensity;
	float StartWind;
	float TargetWind;
	float TargetVolume;
};

// One-shot: assert the wind ambient's AudioComponent VolumeMultiplier is within Tol.
class FTD_AssertAmbientWindVolumeNear : public FTD_Base
{
public:
	FTD_AssertAmbientWindVolumeNear(FAutomationTestBase* InTest, float InExpected, float InTolerance)
		: FTD_Base(InTest), Expected(InExpected), Tolerance(InTolerance) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting Ambient_GlobalWind volume ~= %.2f (+/- %.2f)"), Expected, Tolerance);
	}

	virtual bool UpdateStep() override
	{
		float Value = 0.f;
		if (!StormSkyTest::ReadAmbientWindVolume(E2ELatent::GetPIEWorld(), Value))
		{
			Test->AddError(TEXT("FTD_AssertAmbientWindVolumeNear: could not read Ambient_GlobalWind volume"));
			return true;
		}
		if (FMath::Abs(Value - Expected) > Tolerance)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertAmbientWindVolumeNear: volume is %.3f, expected %.3f (+/- %.2f)"),
				Value, Expected, Tolerance));
		}
		return true;
	}
private:
	float Expected;
	float Tolerance;
};

// One-shot: assert the weather actor's "Wind Intensity - Manual Override" bool.
class FTD_AssertWeatherWindOverride : public FTD_Base
{
public:
	FTD_AssertWeatherWindOverride(FAutomationTestBase* InTest, bool InExpected)
		: FTD_Base(InTest), Expected(InExpected) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting wind Manual Override == %s"), Expected ? TEXT("true") : TEXT("false"));
	}

	virtual bool UpdateStep() override
	{
		bool Value = false;
		if (!StormSkyTest::ReadWeatherWindOverride(E2ELatent::GetPIEWorld(), Value))
		{
			Test->AddError(TEXT("FTD_AssertWeatherWindOverride: could not read 'Wind Intensity - Manual Override'"));
			return true;
		}
		if (Value != Expected)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertWeatherWindOverride: override is %s, expected %s"),
				Value ? TEXT("true") : TEXT("false"), Expected ? TEXT("true") : TEXT("false")));
		}
		return true;
	}
private:
	bool Expected;
};

// One-shot: assert the weather actor's Wind Intensity is within Tolerance of Expected.
class FTD_AssertWeatherWindNear : public FTD_Base
{
public:
	FTD_AssertWeatherWindNear(FAutomationTestBase* InTest, float InExpected, float InTolerance)
		: FTD_Base(InTest), Expected(InExpected), Tolerance(InTolerance) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting weather Wind Intensity ~= %.1f (+/- %.1f)"), Expected, Tolerance);
	}

	virtual bool UpdateStep() override
	{
		float Value = 0.f;
		if (!StormSkyTest::ReadWeatherWind(E2ELatent::GetPIEWorld(), Value))
		{
			Test->AddError(TEXT("FTD_AssertWeatherWindNear: could not read 'Wind Intensity' on a weather actor"));
			return true;
		}
		if (FMath::Abs(Value - Expected) > Tolerance)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertWeatherWindNear: Wind Intensity is %.2f, expected %.2f (+/- %.1f)"),
				Value, Expected, Tolerance));
		}
		return true;
	}
private:
	float Expected;
	float Tolerance;
};

// One-shot: assert the sky's Overall Intensity is within Tolerance of Expected.
class FTD_AssertSkyIntensityNear : public FTD_Base
{
public:
	FTD_AssertSkyIntensityNear(FAutomationTestBase* InTest, float InExpected, float InTolerance)
		: FTD_Base(InTest), Expected(InExpected), Tolerance(InTolerance) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Asserting sky Overall Intensity ~= %.2f (+/- %.2f)"), Expected, Tolerance);
	}

	virtual bool UpdateStep() override
	{
		float Value = 0.f;
		if (!StormSkyTest::ReadSkyIntensity(E2ELatent::GetPIEWorld(), Value))
		{
			Test->AddError(TEXT("FTD_AssertSkyIntensityNear: could not read 'Overall Intensity' on a sky actor"));
			return true;
		}
		if (FMath::Abs(Value - Expected) > Tolerance)
		{
			Test->AddError(FString::Printf(TEXT("FTD_AssertSkyIntensityNear: Overall Intensity is %.3f, expected %.3f (+/- %.2f)"),
				Value, Expected, Tolerance));
		}
		return true;
	}
private:
	float Expected;
	float Tolerance;
};

// Poll until the weather actor's Wind Intensity settles below Below (UDS seeds it
// with a huge sentinel until its first update tick runs). Logs the settled value.
class FTD_WaitForWeatherWindSettled : public FTD_Base
{
public:
	FTD_WaitForWeatherWindSettled(FAutomationTestBase* InTest, float InBelow, double InTimeoutSeconds)
		: FTD_Base(InTest), Below(InBelow), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for weather Wind Intensity to settle below %.1f"), Below);
	}

	virtual bool UpdateStep() override
	{
		float Value = 0.f;
		if (StormSkyTest::ReadWeatherWind(E2ELatent::GetPIEWorld(), Value) && Value <= Below)
		{
			UE_LOG(LogTemp, Log, TEXT("FTD_WaitForWeatherWindSettled: settled at Wind Intensity %.2f"), Value);
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForWeatherWindSettled: Wind Intensity never settled below %.1f within %.1fs (last %.2f)"),
				Below, Timeout, Value));
			return true;
		}
		return false;
	}
private:
	float Below;
	double Timeout;
};

// Poll until the sky's Overall Intensity drops to/below Threshold, erroring on
// timeout. Proves the fade is actually driving the value down over time.
class FTD_WaitForSkyIntensityAtMost : public FTD_Base
{
public:
	FTD_WaitForSkyIntensityAtMost(FAutomationTestBase* InTest, float InThreshold, double InTimeoutSeconds)
		: FTD_Base(InTest), Threshold(InThreshold), Timeout(InTimeoutSeconds) {}

	virtual FString GetStatusText() const override
	{
		return FString::Printf(TEXT("Waiting for sky Overall Intensity <= %.2f"), Threshold);
	}

	virtual bool UpdateStep() override
	{
		float Value = 0.f;
		if (StormSkyTest::ReadSkyIntensity(E2ELatent::GetPIEWorld(), Value) && Value <= Threshold)
		{
			return true;
		}
		if (GetElapsedSinceFirstTick() > Timeout)
		{
			Test->AddError(FString::Printf(TEXT("FTD_WaitForSkyIntensityAtMost: Overall Intensity never reached <= %.2f within %.1fs (last %.3f)"),
				Threshold, Timeout, Value));
			return true;
		}
		return false;
	}
private:
	float Threshold;
	double Timeout;
};

// Diagnostic: log every property on the weather actor whose name contains "Wind",
// with its live runtime value, so we can find the real wind knob (the Details-panel
// "Wind Intensity" sits at a 1e8 sentinel at runtime).
class FTD_DumpWeatherWindProps : public FTD_Base
{
public:
	FTD_DumpWeatherWindProps(FAutomationTestBase* InTest) : FTD_Base(InTest) {}

	virtual FString GetStatusText() const override { return TEXT("Dumping weather wind properties"); }

	virtual bool UpdateStep() override
	{
		UWorld* World = E2ELatent::GetPIEWorld();
		AActor* Weather = nullptr;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->GetClass()->GetName().Contains(TEXT("Ultra_Dynamic_Weather"))) { Weather = *It; break; }
		}
		if (!Weather) { Test->AddError(TEXT("FTD_DumpWeatherWindProps: no weather actor")); return true; }

		UE_LOG(LogTemp, Warning, TEXT("=== WIND PROP DUMP on %s ==="), *Weather->GetClass()->GetName());
		for (TFieldIterator<FProperty> P(Weather->GetClass()); P; ++P)
		{
			const FString N = P->GetName();
			if (!N.Contains(TEXT("Wind"))) { continue; }
			if (FNumericProperty* Num = CastField<FNumericProperty>(*P))
			{
				if (Num->IsFloatingPoint())
				{
					const double V = Num->GetFloatingPointPropertyValue(Num->ContainerPtrToValuePtr<void>(Weather));
					UE_LOG(LogTemp, Warning, TEXT("  [float] '%s' = %.3f"), *N, V);
					continue;
				}
			}
			UE_LOG(LogTemp, Warning, TEXT("  [%s] '%s'"), *P->GetClass()->GetName(), *N);
		}
		UE_LOG(LogTemp, Warning, TEXT("=== END WIND PROP DUMP ==="));
		return true;
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_WindPropDump,
	"Weirdplace2.E2E.Level1.Diagnostic.WindPropDump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_WindPropDump::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("WindPropDump")
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(5.0f)); // let UDW run its update ticks
	ADD_LATENT_AUTOMATION_COMMAND(FTD_DumpWeatherWindProps(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// Full happy-path test
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_HappyPath,
	"Weirdplace2.E2E.Level1.Regression.HappyPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_HappyPath::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("HappyPath")

	// While Seneca is smoking her mesh runs a single-node anim that has no
	// 'ShouldLookAtPlayer' variable, so entering/leaving her look-at sphere logs
	// a (benign) error. Unrelated to the quest flow — expect it.
	AddExpectedError(TEXT("SetShouldLookAtPlayer: 'ShouldLookAtPlayer' not found"), EAutomationExpectedErrorFlags::Contains, 0);

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
	E2ESteps::OpenBathroomDoor(this);
	E2ESteps::EnterStall(this);
	E2ESteps::ExitBathroom(this);

	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertHasItem(this, FName("BrokenKey")));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// StormSkyFade — the KeyBroke beat fades Ultra_Dynamic_Sky's Overall Intensity
// from ~2.0 down to ~0.25, darkening the scene into a storm mood.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_StormSkyFade,
	"Weirdplace2.E2E.Level1.Regression.StormSkyFade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_StormSkyFade::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("StormSkyFade")

	// Compress the 2-minute default transition to 2s so the test is quick. Wind ramps
	// 100 -> 250 and the wind ambient swells -> 3.0 alongside the intensity fade 2.0 -> 0.25.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_ConfigureStormSkyFade(this, /*Duration*/ 2.0f, /*Intensity*/ 0.25f, /*StartWind*/ 100.0f, /*TargetWind*/ 250.0f, /*TargetVolume*/ 3.0f));

	// Before the beat: warning unseen, sky at its authored brightness (~2.0), wind
	// ambient at its base volume (~1.0). (We can't assert the pre-storm Wind Intensity:
	// this level leaves Manual Override on and UDW parks the value at a ~1e8 sentinel.)
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("SeenTornadoWarning"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSkyIntensityNear(this, 2.0f, 0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientWindVolumeNear(this, 1.0f, 0.2f));

	// See the warning — the controller fades the sky down, ramps the wind up (engaging
	// UDW's manual wind override), and swells the wind ambient volume.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	// Wait until the fade is essentially complete (~99%) so wind/volume have landed.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSkyIntensityAtMost(this, 0.26f, 8.0));

	// Settled at the storm targets (sky waited above; all ramp over the same window).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSkyIntensityNear(this, 0.25f, 0.1f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertWeatherWindOverride(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertWeatherWindNear(this, 250.0f, 20.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientWindVolumeNear(this, 3.0f, 0.2f));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// BathroomDoorTraceRepro — standalone diagnostic
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_BathroomDoorTraceRepro,
	"Weirdplace2.E2E.Level1.Diagnostic.BathroomDoorTraceRepro",
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
// BathroomKeypadRules — guards the employee-bathroom keypad's acceptance
// rules. The lock is mostly an illusion (almost any 4-digit entry opens it
// once the payphone's been used), but a valid entry MUST contain an 8 and
// MUST NOT be a blocked "obvious" code. A rejected entry buzzes + clears but
// keeps the pad up and the door shut.
//   "4729" — no 8     -> rejected (door locked, deny #1, pad stays open)
//   "8888" — blocked  -> rejected (door locked, deny #2)
//   "4289" — valid    -> opens
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_BathroomKeypadRules,
	"Weirdplace2.E2E.Level1.Regression.BathroomKeypadRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_BathroomKeypadRules::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("BathroomKeypadRules")

	// The keypad only accepts anything once the player has used the payphone.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("UsedPayPhone"), true));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("EmployeeBathroom")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BathroomDoor")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForKeypadOpen(this));

	// "4729" has no 8 -> rejected. Wait past WrongCodeClearDelay (0.5s) for the
	// buzz + entry clear; the door stays locked and the pad stays up.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_EnterKeypadCode(this, TEXT("4729")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertKeypadDenyCount(this, 1));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertKeypadOpen(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertDoorClosed(this, TEXT("BathroomDoor")));

	// "8888" contains 8s but is a blocked obvious code -> rejected.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_EnterKeypadCode(this, TEXT("8888")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertKeypadDenyCount(this, 2));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertDoorClosed(this, TEXT("BathroomDoor")));

	// "4289" contains an 8 and isn't blocked -> opens.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_EnterKeypadCode(this, TEXT("4289")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForDoorOpen(this, TEXT("BathroomDoor")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// LockSoundDuringKeyInsert — re-entrancy guard regression. A repeated interact
// fired WHILE the key-break sequence is running (the UE5.7 double-fire input
// quirk turns one key-insert press into two) must NOT fall into the locked
// rattle branch. The break sequence removes the Key + clears the active item
// immediately, but doesn't set bDidDropKey until ~3s later; without the guard
// the re-entrant interact sees "no active key" and plays LockedDoorSound.
//
// RED (no guard): locked-sound count == 1 after the re-entrant interact.
// GREEN (guard):  count stays 0.
//
// Also verifies the self-illumination glow follows the key through the whole
// sequence: on the animated key while inserted, and on the dropped broken-key
// pickup where it lands, so both read in the dark.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_LockSoundDuringKeyInsert,
	"Weirdplace2.E2E.Level1.Regression.LockSoundDuringKeyInsert",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_LockSoundDuringKeyInsert::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("LockSoundDuringKeyInsert")

	// Grant the Key; having it in inventory is enough — the first interact starts
	// the break sequence instead of rattling.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("Key"),
		TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("OutsideBathroom")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorComponentByName(this, TEXT("BP_OutsideBathroomDoor"), TEXT("KeyLockSocket")));

	// Interact #1 pops the inventory (give mode); select the Key and press E to
	// give it, which starts the key-break sequence (removes Key).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.6f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	// Let the animated key appear at the lock — it should carry the same glow
	// overlay the held key had, so it reads in the dark while inserted.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertBathroomDoorAnimKeyGlow(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_KeyInsert_Glow")));
	// Still mid-sequence — bDidDropKey is false (set ~3s in on broken-key spawn).
	// Interact #2: the re-entrant press. RED falls into the locked rattle.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_LockSound_MidKeyBreak")));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertBathroomDoorLockedSoundCount(this, 0));

	// Let the sequence finish and drop the broken-key pickup, then confirm it
	// glows where it lands so it's findable on the dark floor (not just once
	// it's pulled in to inspect).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForInspectablePickupSpawned(this, 15.0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInspectablePickupGlow(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearInspectablePickupAndAim(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_BrokenKey_GroundGlow")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// SenecaSmokingAnim — diagnostic for the smoking animation failing to
// play on the natural quest path (works via SkipToSmoking console cmd).
// Lingers around Seneca's appear-at-smoking-spot moment and takes
// screenshots at T+1s/T+3s/T+5s so we can see whether her arms come up.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_SenecaSmokingAnim,
	"Weirdplace2.E2E.Level1.Diagnostic.SenecaSmokingAnim",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_SenecaSmokingAnim::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SenecaSmokingAnim")

	E2ESteps::SenecaIntro(this);
	E2ESteps::CollectMovies(this);
	E2ESteps::GiveMoviesToSeneca(this);
	E2ESteps::GetMoneyFromRick(this);
	E2ESteps::GiveMoneyAskForBlank(this);
	E2ESteps::CollectBlankTape(this);
	E2ESteps::GiveBlankTapeGetKey(this);
	E2ESteps::UseKeyOnDoor(this);
	E2ESteps::FastForwardSenecaSmoking(this);

	// Linger + screenshot around the appear moment
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaSmoking")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaAppearedAtSmoking(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("Diag_SenecaSmoking_T+1s")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("Diag_SenecaSmoking_T+3s")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("Diag_SenecaSmoking_T+5s")));

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
	"Weirdplace2.E2E.Level1.Regression.DialogueCooldown",
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
	"Weirdplace2.E2E.Level1.Diagnostic.SensitivityScaling",
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
	"Weirdplace2.E2E.Level1.Regression.PauseMenu",
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
// PauseMenuLight — the menu/inventory UI is fully self-illuminated (emissive
// panels/thumbnails + unlit M_UnlitText for the labels), so the player's
// inventory RectLight is intentionally never enabled. This guards that the menu
// does NOT turn the light on; the screenshot confirms it reads with the light off.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_PauseMenuLight,
	"Weirdplace2.E2E.Level1.Regression.PauseMenuLight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_PauseMenuLight::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("PauseMenuLight")

	// Baseline: light off before any UI is opened.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryFlashlight(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenuLight_01_Before")));

	// Open the menu and wait past the open animation. The light must stay OFF —
	// the menu self-illuminates (unlit text material), so it's never enabled.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateSettingsPress(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::Interacting));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryFlashlight(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PauseMenuLight_02_MenuOpenLightOff")));

	// Close the menu — light stays off.
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
	"Weirdplace2.E2E.Level1.Regression.InventoryThumbnails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_InventoryThumbnails::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("InventoryThumbnails")

	// Inject Money + Key directly so we can focus on rendering, not gameplay.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("Money"),
		TEXT("/Game/Import/cash/money.money"), FVector(1.0f)));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("Key"),
		TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));
	// A movie item: its ItemID resolves to /Game/VHSCovers/<ItemID>, exercising
	// the M_VHSCoverFront thumbnail material (the mesh path is irrelevant to the
	// thumbnail). Verifies VHS covers self-illuminate without the inventory light.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName("BATMAN-THE-MOVIE"),
		TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 3));

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
// InventoryHorizontalScroll — the inventory is an unbounded single-row strip
// that scrolls horizontally. Inject more items than the visible window, then
// jump the cursor to the last item and back, asserting the scroll window
// follows the selection (proves it scrolls rather than clamping at the edge).
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_InventoryHorizontalScroll,
	"Weirdplace2.E2E.Level1.Regression.InventoryHorizontalScroll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_InventoryHorizontalScroll::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("InventoryHorizontalScroll")

	// Inject more items than any reasonable visible window so the strip must scroll.
	const int32 NumItems = 12;
	for (int32 i = 0; i < NumItems; ++i)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this,
			FName(*FString::Printf(TEXT("ScrollItem%02d"), i)),
			TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, NumItems));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	// Window starts at the left edge: selection 0, no scroll.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSelectedSlot(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertScrollOffset(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Scroll_01_Start")));

	// Jump to the last item — the strip must scroll right to keep it visible.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, NumItems - 1));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertScrollWindow(this, NumItems - 1, /*bExpectScrolled*/ true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Scroll_02_Scrolled")));

	// Jump back to the first item — window snaps back to the left.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SelectAndConfirmSlot(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSelectedSlot(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertScrollOffset(this, 0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Scroll_03_BackToStart")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// VhsCoverLetterbox — VHS movie covers are portrait but the inventory slots are
// square (1:1). M_VHSCoverFront pillarboxes the cover (fit to height, black bars
// left/right) so it isn't stretched. Inject a few real covers and screenshot;
// each should be upright, undistorted (taller than wide), with side bars.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_VhsCoverLetterbox,
	"Weirdplace2.E2E.Level1.Diagnostic.VhsCoverLetterbox",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_VhsCoverLetterbox::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("VhsCoverLetterbox")

	// ItemID resolves to /Game/VHSCovers/<ItemID> -> M_VHSCoverFront. The mesh
	// path is irrelevant to the thumbnail (reuse the key mesh).
	const TCHAR* Covers[] = { TEXT("BATMAN-THE-MOVIE"), TEXT("RIVALS"), TEXT("ABRAXAS") };
	for (const TCHAR* Cover : Covers)
	{
		ADD_LATENT_AUTOMATION_COMMAND(FTD_AddTestItem(this, FName(Cover),
			TEXT("/Game/Fab/Small_Key__1MB_/small_key_1mb.small_key_1mb"), FVector(0.001f)));
	}
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 3));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_VhsLetterbox")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// GazeReward — staring at the one rigged gas-station canopy light
// ('gasstationbarlight') for 30 continuous seconds grants more cash
// (the Money item). While the gaze is held, a hum swells toward the
// 30-second mark and stops once the reward fires. The other canopy
// lights are NOT rigged and must grant nothing.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_GazeReward,
	"Weirdplace2.E2E.Level1.Regression.GazeReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_GazeReward::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("GazeReward")

	// --- The rigged light: stare 30s -> Money, hum rising on the way ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertNotHasItem(this, FName("Money")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("gasstationbarlight"), 500.f));
	// The canopy light sits at Z~821; the teleport drops the player onto the
	// lot below — let them land before aiming.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("gasstationbarlight")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_GazeReward_01_StaringAtLight")));
	// Hum volume sampled at ~5s and ~15s into the stare must be rising.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeHumRising(this, 5.0, 15.0));
	// ~15s in, the screen-space effect has ramped on (but not yet to max).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeEffectWeight(this, TEXT("mid-stare"), 0.005f, 0.81f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_GazeReward_03_EffectMidStare")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForItemAdded(this, FName("Money"), 40.0));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeHumStopped(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_GazeReward_02_CashGranted")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 1));

	// --- Only that one light is rigged: a long stare at another canopy
	// light must grant nothing. ---
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("gasstationbarlight2"), 500.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("gasstationbarlight2")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(35.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertInventoryCount(this, 1));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// GazeRewardReset — the UGazeRewardComponent's dwell timer accumulates while
// the player looks at the owner and resets the instant the gaze breaks. Fast
// (no 30s wait, no grant): sample seconds rising, look away, sample reset.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_GazeRewardReset,
	"Weirdplace2.E2E.Level1.Regression.GazeRewardReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_GazeRewardReset::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("GazeRewardReset")

	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("gasstationbarlight"), 500.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("gasstationbarlight")));
	// Wait for the player to land and the gaze to hold (well short of the 30s grant).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForGazeSeconds(this, 1.5f, 12.0));
	// The dwell timer is accumulating, the screen effect has begun ramping on,
	// and the camera has zoomed in slightly (FOV below the base 90).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeRewardSeconds(this, TEXT("during gaze"), 1.0f, 30.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeEffectWeight(this, TEXT("during gaze"), 0.001f, 0.81f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeCameraFOV(this, TEXT("during gaze"), 70.0f, 89.95f));
	// Look away (down at the lot) — the timer, effect, and FOV must snap back.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookDown(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeRewardSeconds(this, TEXT("after look-away"), 0.0f, 0.001f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeEffectWeight(this, TEXT("after look-away"), 0.0f, 0.001f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertGazeCameraFOV(this, TEXT("after look-away"), 89.9f, 90.1f));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// RickDialoguePrefix — Rick's outside-idle line is authored as
// "Rick: I'll meet you inside once I'm done here". The speaker belongs on
// the plate; the body shown to the player must not include "Rick:".
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_RickDialoguePrefix,
	"Weirdplace2.E2E.Level1.Diagnostic.RickDialoguePrefix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_RickDialoguePrefix::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("RickDialoguePrefix")

	// Same approach as HappyPath's GetMoneyFromRick: the RickApproach waypoint
	// is placed where the interact trace reliably reaches him.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("RickApproach")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtRick(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InSimpleDialogue));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertDialogueLine(this,
		TEXT("Rick"), TEXT("I'll meet you inside once I'm done here")));

	// Let the typewriter draw before the screenshot.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_RickPrefix_DialogueShown")));

	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueViaInput(this, EPlayerActivityState::FreeRoaming));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// MoviePutBackPrompt — while inspecting a movie, a world-space prompt
// names the put-back binding (Exit Interaction: Q / gamepad B) and keeps
// facing the player even as the box is rotated. Pressing the prompted key
// actually puts the movie back.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_MoviePutBackPrompt,
	"Weirdplace2.E2E.Level1.Regression.MoviePutBackPrompt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_MoviePutBackPrompt::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("MoviePutBackPrompt")

	// Same approach as HappyPath's CollectMovies: stand in front of the box
	// and aim at a trace-verified surface point.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportFacingShelfBoxAndAim(this, TEXT("BP_MovieBox120")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::Interacting));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));

	// Mouse drives this test, so the prompt must show ONLY the keyboard key.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPutBackPrompt(this, TEXT("[Q]  put back")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PutBack_01_PromptShown")));

	// Rotate the box (mouse axis); the prompt must keep facing the camera
	// and stay keyboard-flavored.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_InjectMouseXForDuration(this, 90.0f, 0.6f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPutBackPrompt(this, TEXT("[Q]  put back")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PutBack_02_AfterRotate")));

	// The prompted key really does put the movie back.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateKeyPress(this, EKeys::Q));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::FreeRoaming));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// BlankVhsGazeSweep — diagnostic. Activate the blank tape, stand in front
// of it, then sweep the reticle across a grid over its face, logging what
// the chord gaze trace hits at each point (and screenshotting the corners).
// Gathers objective data on the spotty look-to-boost-volume behavior.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_BlankVhsGazeSweep,
	"Weirdplace2.E2E.Level1.Diagnostic.BlankVhsGazeSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_BlankVhsGazeSweep::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("BlankVhsGazeSweep")

	// Spawn the blank VHS as the chosen box + start the chord (sets up the
	// SpawnerActorComponent's gaze trace), then stand in front of it.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_ActivateBlankTape(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.3f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearBlankTape(this));

	// Sweep a 5x5 grid across the face, logging the gaze trace at each point.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SweepGazeOverBlankVhs(this, 5));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// InventoryFromStart — the inventory works from the moment the player
// spawns; no Seneca-intro gate, no test bypass.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_InventoryFromStart,
	"Weirdplace2.E2E.Level1.Regression.InventoryFromStart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_InventoryFromStart::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("InventoryFromStart")

	// Deliberately NO unlock step: Tab must work at spawn.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_OpenInventoryViaInput(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_InvFromStart_Open")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CloseInventoryViaInput(this));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// StoryFlags — minimal infra guard for the central UStorySubsystem. Set a
// flag through the test hook and confirm IsStoryFlagSet reflects it. The
// flags' gameplay *effects* are proven by items 1/2/4/5; this just guards
// the store/read plumbing the whole chain depends on.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_StoryFlags,
	"Weirdplace2.E2E.Level1.Regression.StoryFlags",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_StoryFlags::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("StoryFlags")

	// Fresh world: the flag starts clear.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("SeenTornadoWarning"), false));
	// Set it through the hook, then it must read back true.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("SeenTornadoWarning"), true));
	// A different flag set independently doesn't bleed across.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("KeyBroke"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("KeyBroke"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("KeyBroke"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("SeenTornadoWarning"), true));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// SenecaShelterLine (item 4) — Seneca's Smoking dialogue gains a tornado-
// shelter tip, but only once the player has seen the tornado warning
// (SeenTornadoWarning). Hook-asserts presence/absence by flag, then drives
// her into the Smoking beat to screenshot the line on the dialogue widget.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_SenecaShelterLine,
	"Weirdplace2.E2E.Level1.Regression.SenecaShelterLine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_SenecaShelterLine::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("SenecaShelterLine")

	// Seneca's smoking mesh runs a single-node anim with no 'ShouldLookAtPlayer'
	// var — entering her look-at sphere logs a benign error. Same as HappyPath.
	AddExpectedError(TEXT("SetShouldLookAtPlayer: 'ShouldLookAtPlayer' not found"), EAutomationExpectedErrorFlags::Contains, 0);

	// Gating: with the flag unset, the shelter tip is absent.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSenecaSmokingLines(this, TEXT("shelter"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSenecaSmokingLines(this, TEXT("stall"), false));

	// Set SeenTornadoWarning → the shelter tip appears in the Smoking lines.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSenecaSmokingLines(this, TEXT("shelter"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertSenecaSmokingLines(this, TEXT("stall"), true));

	// Screenshot: drive Seneca into Smoking, open dialogue, advance to the
	// shelter line, and capture it on the widget.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_ForceSenecaSmoking(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportTo(this, TEXT("SenecaSmoking")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForSenecaAppearedAtSmoking(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtSeneca(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SimulateInteractAction(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForActivityState(this, EPlayerActivityState::InSimpleDialogue));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AdvanceDialogueUntilLineContains(this, TEXT("shelter")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(1.2f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_SenecaShelterLine_Dialogue")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// TornadoWarningOnStoreEntry (item 1) — re-entering the store after the
// bathroom key breaks switches both store TVs to a tornado-warning screen,
// and gazing at one for the dwell registers SeenTornadoWarning.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_TornadoWarningOnStoreEntry,
	"Weirdplace2.E2E.Level1.Regression.TornadoWarningOnStoreEntry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_TornadoWarningOnStoreEntry::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("TornadoWarningOnStoreEntry")

	// Without KeyBroke, store entry must NOT switch the TVs.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerStoreEntry(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvShowingWarning(this, TEXT("BP_TV"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvShowingWarning(this, TEXT("BP_TV2"), false));

	// After the key breaks, store entry switches BOTH TVs and records the beat.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("KeyBroke"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerStoreEntry(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvShowingWarning(this, TEXT("BP_TV"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvShowingWarning(this, TEXT("BP_TV2"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("TornadoWarningDisplayed"), true));

	// Not "seen" yet — the player hasn't looked at a warning TV.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertStoryFlag(this, FName("SeenTornadoWarning"), false));

	// Screenshot the warning screen.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("BP_TV"), 250.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BP_TV")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_TornadoWarning_Screen")));

	// Holding the gaze on a warning TV for the dwell sets SeenTornadoWarning.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_WaitForStoryFlag(this, FName("SeenTornadoWarning"), true, 8.0));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// TornadoWarningStormBeat — when the store TVs switch to the tornado warning,
// the storm closes in: both TVs blare a looping siren, the store's TV ambient
// beds cut out, and the referenced gas-station light dims (and stays dimmed).
// The placed AStormBeatController is designer config, so the test self-configures
// one at runtime BEFORE triggering store entry (PIE spawn is safe; it's headless
// EDITOR spawn of C++ classes that crashes in 5.7).
// =======================================================================

namespace
{
	static float StormBeat_BaselineLightIntensity = 0.f;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_TornadoWarningStormBeat,
	"Weirdplace2.E2E.Level1.Regression.TornadoWarningStormBeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_TornadoWarningStormBeat::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("TornadoWarningStormBeat")

	// Reset the capture so re-runs in the same editor session don't compare
	// against a previous run's leftover baseline.
	StormBeat_BaselineLightIntensity = 0.f;

	// A gas-station canopy spotlight (100 lm). Stable label confirmed in-level.
	const TCHAR* GasLight = TEXT("SpotLight3");
	// An emissive "light" mesh (no light component) — hidden, not dimmed.
	const TCHAR* GlowMesh = TEXT("outsidegastationlights");

	// --- Baseline (before the beat) ---
	// The store TV ambient beds are playing, neither TV is blaring its siren, the
	// gas-station light is at full intensity (captured for the dim assert), and the
	// emissive glow mesh is still visible.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientPlaying(this, TEXT("Ambient_TV"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientPlaying(this, TEXT("Ambient_TV2"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_CaptureLightIntensity(this, GasLight, &StormBeat_BaselineLightIntensity));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV2"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertActorVisible(this, GlowMesh, true));

	// Self-configure the storm controller BEFORE the flag fires so it's subscribed.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SpawnStormBeat(this,
		{ FString(GasLight) },
		{ FString(GlowMesh) },
		{ FString(TEXT("Ambient_TV")), FString(TEXT("Ambient_TV2")) },
		/*Mult=*/0.3f));

	// Fire the beat: key broke, then re-enter the store.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("KeyBroke"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerStoreEntry(this));

	// Let the audio engine start the looping sirens and stop the ambient beds.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));

	// (RED-defining) Both TVs blare the looping tornado-alert siren.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV2"), true));

	// (RED-defining) The store's TV ambient beds cut out.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientPlaying(this, TEXT("Ambient_TV"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertAmbientPlaying(this, TEXT("Ambient_TV2"), false));

	// (RED-defining) The gas-station light dimmed to ~ baseline * 0.3 and stays there.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertLightDimmed(this, GasLight,
		&StormBeat_BaselineLightIntensity, /*Mult=*/0.3f, /*Tolerance=*/1.0f));

	// (RED-defining) The emissive "light" mesh (no light component) is hidden.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertActorVisible(this, GlowMesh, false));

	// --- Using the payphone silences the sirens (screens stay up) ---
	// The phone only answers once the warning's been seen; set that, pick it up,
	// and both sirens cut out. The warning screens are a material swap on the
	// always-visible TV actors, so they remain — documented by the screenshot below.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerPayPhonePickup(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV"), false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertTvWarningAudio(this, TEXT("BP_TV2"), false));

	// Document the alarm/dim moment (the screen texture is designer art → the red
	// fallback shows here until the designer assigns WarningScreenTexture).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("BP_TV"), 250.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BP_TV")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_TornadoStormBeat_Screen")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// TelephoneGatedOnWarning (item 2) — the roadside telephone scene
// (BP_TelephoneScene, reparented onto APayPhone) stays hidden until the
// player has seen the tornado warning, then reveals.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_TelephoneGatedOnWarning,
	"Weirdplace2.E2E.Level1.Regression.TelephoneGatedOnWarning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_TelephoneGatedOnWarning::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("TelephoneGatedOnWarning")

	// Gated: with SeenTornadoWarning unset, the scene root is hidden.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertActorVisible(this, TEXT("BP_TelephoneScene"), false));

	// Seeing the warning reveals it.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertActorVisible(this, TEXT("BP_TelephoneScene"), true));

	// Screenshot to eyeball pole + payphone materials after the reparent.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("BP_TelephoneScene"), 450.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BP_TelephoneScene")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_Telephone_Revealed")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

// =======================================================================
// PayPhoneDialtone (item 5) — once revealed (SeenTornadoWarning), interacting
// picks up the receiver: a one-shot pickup, then a looping dialtone (with the
// static/voices over it). The player is held at the phone until "Exit
// Interaction" hangs up — stopping the dialtone and releasing them. Repeatable.
// =======================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FE2E_Level1_PayPhoneDialtone,
	"Weirdplace2.E2E.Level1.Regression.PayPhoneDialtone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FE2E_Level1_PayPhoneDialtone::RunTest(const FString& Parameters)
{
	E2E_TEST_PREAMBLE("PayPhoneDialtone")

	// Without the flag: gated off, pickup does nothing.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneCanInteract(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerPayPhonePickup(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneAudioPlaying(this, false));

	// With the flag: can pick up.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_SetStoryFlag(this, FName("SeenTornadoWarning"), true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneCanInteract(this, true));

	// Mark the code already heard so this is a mundane "dialtone only" call — the
	// persistent looping dialtone. (On a FIRST call the dialtone deliberately cuts
	// out ~1.5s in, before the spoken code; that transient sequence isn't what this
	// pickup/hangup/repeat mechanic test is about.)
	ADD_LATENT_AUTOMATION_COMMAND(FTD_MarkPayPhoneCodeSpoken(this));

	// Pick up: pickup one-shot plays immediately, and we're now off the hook
	// so a re-pickup is blocked.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerPayPhonePickup(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneAudioPlaying(this, true));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneCanInteract(this, false));

	// After the 0.52s pickup, the dialtone loop has started — and on a mundane
	// call it keeps looping (no cut), so the assert is timing-robust.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.7f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneDialtone(this, true));

	// Hang up: dialtone stops and we can pick up again (repeatable).
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TriggerPayPhoneHangUp(this));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneDialtone(this, false));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_AssertPayPhoneCanInteract(this, true));

	// Screenshot of the revealed phone.
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TeleportNearActorByLabel(this, TEXT("BP_TelephoneScene"), 300.f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_Delay(0.5f));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_LookAtActorByLabel(this, TEXT("BP_TelephoneScene")));
	ADD_LATENT_AUTOMATION_COMMAND(FTD_TakeScreenshot(TEXT("E2E_PayPhoneDialtone")));

	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
