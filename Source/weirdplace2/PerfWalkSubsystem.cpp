#include "PerfWalkSubsystem.h"

#include "TestDriverSubsystem.h"
#include "TestWaypoint.h"
#include "FirstPersonCharacter.h"
#include "GameFramework/Controller.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

// ---------------------------------------------------------------------------
// Console command: weirdplace.PerfWalk [nocsv] [noquit]
// File-scope registration so it exists at module load. Resolves the game world
// from the passed context, falling back to GEngine's world contexts in case the
// console context world is null at -ExecCmds startup time.
// ---------------------------------------------------------------------------
static void PerfWalkConsole(const TArray<FString>& Args, UWorld* InWorld)
{
	UWorld* World = InWorld;
	if (!World && GEngine)
	{
		for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
		{
			if ((Ctx.WorldType == EWorldType::Game || Ctx.WorldType == EWorldType::PIE) && Ctx.World())
			{
				World = Ctx.World();
				break;
			}
		}
	}
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] no game world available"));
		return;
	}
	UPerfWalkSubsystem* Sys = World->GetSubsystem<UPerfWalkSubsystem>();
	if (!Sys)
	{
		UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] no PerfWalkSubsystem"));
		return;
	}
	FPerfWalkOptions Opts;
	Opts.bCsvProfile = !Args.Contains(TEXT("nocsv"));
	Opts.bQuitOnDone = !Args.Contains(TEXT("noquit"));
	Sys->Arm(Opts);
}

static FAutoConsoleCommandWithWorldAndArgs GPerfWalkCmd(
	TEXT("weirdplace.PerfWalk"),
	TEXT("Run the automated perf walk for profiling. Optional args: nocsv noquit"),
	FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&PerfWalkConsole));

// ---------------------------------------------------------------------------

void UPerfWalkSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Headless auto-arm: launch the cooked build with "-PerfWalk" (plus optional
	// "-PerfWalkNoCsv" / "-PerfWalkNoQuit"). More robust than -ExecCmds because it
	// doesn't depend on the console-command world being valid at startup.
	if (FParse::Param(FCommandLine::Get(), TEXT("PerfWalk")))
	{
		FPerfWalkOptions O;
		O.bCsvProfile = !FParse::Param(FCommandLine::Get(), TEXT("PerfWalkNoCsv"));
		O.bQuitOnDone = !FParse::Param(FCommandLine::Get(), TEXT("PerfWalkNoQuit"));
		Arm(O);
	}
}

bool UPerfWalkSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

bool UPerfWalkSubsystem::IsTickable() const
{
	return Phase == EPhase::Settling || Phase == EPhase::WaitingForPlayer || Phase == EPhase::Running;
}

TStatId UPerfWalkSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPerfWalkSubsystem, STATGROUP_Tickables);
}

UTestDriverSubsystem* UPerfWalkSubsystem::GetDriver() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTestDriverSubsystem>() : nullptr;
}

void UPerfWalkSubsystem::Exec(const TCHAR* Cmd) const
{
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), Cmd);
	}
}

void UPerfWalkSubsystem::Arm(const FPerfWalkOptions& InOpts)
{
	Opts = InOpts;
	BuildSteps();
	StepIndex = 0;
	Elapsed = 0.f;
	bStepInit = false;
	Phase = EPhase::Settling;
	UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] armed: %d steps (csv=%d quit=%d settle=%.1fs)"),
		Steps.Num(), Opts.bCsvProfile ? 1 : 0, Opts.bQuitOnDone ? 1 : 0, Opts.SettleSeconds);
}

void UPerfWalkSubsystem::BuildSteps()
{
	// Mirrors the editor FE2E_Level1_PerfWalkProfile sequence.
	auto Teleport = [](FName T) { return FPerfWalkStep{ EPerfWalkStepType::Teleport, T, 0.f, 0.f }; };
	auto Lerp = [](FName T, float D) { return FPerfWalkStep{ EPerfWalkStepType::Lerp, T, D, 0.f }; };
	auto Yaw = [](float D) { return FPerfWalkStep{ EPerfWalkStepType::YawSweep, NAME_None, D, 360.f }; };

	Steps.Reset();
	Steps.Add(Teleport(TEXT("RickApproach")));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Lerp(TEXT("SenecaApproach"), 4.0f));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Lerp(TEXT("MovieShelf"), 2.5f));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Lerp(TEXT("SenecaSmoking"), 4.0f));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Lerp(TEXT("SenecaHallway"), 3.0f));
	Steps.Add(Lerp(TEXT("EmployeeBathroom"), 2.5f));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Lerp(TEXT("OutsideBathroom"), 3.0f));
	Steps.Add(Lerp(TEXT("ApproachStall"), 2.5f));
	Steps.Add(Lerp(TEXT("Teleporter"), 2.5f));
	Steps.Add(Yaw(4.0f));
	Steps.Add(Teleport(TEXT("OasisCenter")));
	Steps.Add(Yaw(5.0f));
	Steps.Add(Teleport(TEXT("OasisDoor")));
	Steps.Add(Yaw(4.0f));
}

void UPerfWalkSubsystem::Tick(float DeltaTime)
{
	switch (Phase)
	{
	case EPhase::Settling:
		Elapsed += DeltaTime;
		if (Elapsed >= Opts.SettleSeconds)
		{
			Phase = EPhase::WaitingForPlayer;
			Elapsed = 0.f;
		}
		break;

	case EPhase::WaitingForPlayer:
	{
		UTestDriverSubsystem* Driver = GetDriver();
		if (Driver && Driver->IsPlayerReady())
		{
			if (Opts.bCsvProfile)
			{
				Exec(TEXT("CsvProfile start"));
			}
			StepIndex = 0;
			Elapsed = 0.f;
			bStepInit = false;
			Phase = EPhase::Running;
			UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] player ready -> running"));
		}
		break;
	}

	case EPhase::Running:
		if (!bStepInit)
		{
			BeginStep();
			bStepInit = true;
		}
		Elapsed += DeltaTime;
		if (TickStep(DeltaTime))
		{
			++StepIndex;
			Elapsed = 0.f;
			bStepInit = false;
			if (StepIndex >= Steps.Num())
			{
				if (Opts.bCsvProfile)
				{
					Exec(TEXT("CsvProfile stop"));
				}
				Phase = EPhase::Finished;
				UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] finished"));
				if (Opts.bQuitOnDone)
				{
					Exec(TEXT("quit"));
				}
			}
		}
		break;

	default:
		break;
	}
}

void UPerfWalkSubsystem::BeginStep()
{
	const FPerfWalkStep& S = Steps[StepIndex];
	UTestDriverSubsystem* Driver = GetDriver();
	AFirstPersonCharacter* Player = Driver ? Driver->GetPlayer() : nullptr;
	if (!Player)
	{
		return;
	}
	AController* Controller = Player->GetController();

	UE_LOG(LogTemp, Warning, TEXT("[PerfWalk] step %d/%d type=%d tag=%s"),
		StepIndex + 1, Steps.Num(), static_cast<int32>(S.Type), *S.Tag.ToString());

	switch (S.Type)
	{
	case EPerfWalkStepType::Teleport:
		if (Driver)
		{
			Driver->TeleportPlayerToWaypoint(S.Tag);
		}
		break;

	case EPerfWalkStepType::Lerp:
	{
		ATestWaypoint* WP = ATestWaypoint::FindByTag(Player, S.Tag);
		StartPos = Player->GetActorLocation();
		EndPos = WP ? WP->GetActorLocation() : StartPos;
		EndPos.Z = StartPos.Z; // walk flat
		Player->SetActorEnableCollision(false);
		if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
		{
			Move->SetMovementMode(MOVE_None);
		}
		break;
	}

	case EPerfWalkStepType::YawSweep:
		StartYaw = Controller ? Controller->GetControlRotation().Yaw : 0.f;
		break;
	}
}

bool UPerfWalkSubsystem::TickStep(float DeltaTime)
{
	const FPerfWalkStep& S = Steps[StepIndex];
	UTestDriverSubsystem* Driver = GetDriver();
	AFirstPersonCharacter* Player = Driver ? Driver->GetPlayer() : nullptr;
	if (!Player)
	{
		return true; // player gone — skip
	}

	switch (S.Type)
	{
	case EPerfWalkStepType::Teleport:
		return true; // performed in BeginStep

	case EPerfWalkStepType::Lerp:
	{
		if (S.Duration <= 0.f)
		{
			Player->SetActorLocation(EndPos, false, nullptr, ETeleportType::TeleportPhysics);
			Player->SetActorEnableCollision(true);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
			}
			return true;
		}
		const float Alpha = FMath::Clamp(Elapsed / S.Duration, 0.f, 1.f);
		Player->SetActorLocation(FMath::Lerp(StartPos, EndPos, Alpha), false, nullptr, ETeleportType::TeleportPhysics);
		if (Alpha >= 1.f)
		{
			Player->SetActorEnableCollision(true);
			if (UCharacterMovementComponent* Move = Player->GetCharacterMovement())
			{
				Move->SetMovementMode(MOVE_Walking);
			}
			return true;
		}
		return false;
	}

	case EPerfWalkStepType::YawSweep:
	{
		AController* Controller = Player->GetController();
		if (!Controller)
		{
			return true;
		}
		const float Alpha = S.Duration > 0.f ? FMath::Clamp(Elapsed / S.Duration, 0.f, 1.f) : 1.f;
		FRotator Rot = Controller->GetControlRotation();
		Rot.Yaw = StartYaw + S.Degrees * Alpha;
		Controller->SetControlRotation(Rot);
		return Alpha >= 1.f;
	}
	}

	return true;
}
