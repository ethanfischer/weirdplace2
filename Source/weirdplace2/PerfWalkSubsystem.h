#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PerfWalkSubsystem.generated.h"

class UTestDriverSubsystem;

// One step of the automated perf walk. Plain (non-reflected) — internal only.
enum class EPerfWalkStepType : uint8
{
	Teleport,   // instant teleport to a waypoint tag
	Lerp,       // noclip lerp to a waypoint tag over Duration
	YawSweep    // rotate view yaw by Degrees over Duration
};

struct FPerfWalkStep
{
	EPerfWalkStepType Type = EPerfWalkStepType::Teleport;
	FName Tag = NAME_None;
	float Duration = 0.f;
	float Degrees = 360.f;
};

struct FPerfWalkOptions
{
	bool bCsvProfile = true;   // CsvProfile start/stop around the walk
	bool bQuitOnDone = true;   // exec "quit" when finished (finalizes a .utrace for headless runs)
	float SettleSeconds = 2.f; // let first-frame load spikes pass before capturing
};

// Runtime-available (cooked builds too) driver that reproduces the editor
// Diagnostic.PerfWalkProfile test: noclip-walks the level waypoints with yaw
// sweeps so hitches can be profiled in a packaged Development build.
// Trigger headless with the "-PerfWalk" command-line switch, or interactively
// with the "weirdplace.PerfWalk" console command.
UCLASS()
class WEIRDPLACE2_API UPerfWalkSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UWorldSubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// FTickableGameObject (via UTickableWorldSubsystem)
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	// Begin the walk. Safe to call before the player pawn exists.
	void Arm(const FPerfWalkOptions& InOpts);

private:
	enum class EPhase : uint8 { Idle, Settling, WaitingForPlayer, Running, Finished };

	void BuildSteps();
	void BeginStep();
	bool TickStep(float DeltaTime);   // returns true when the current step is done
	void Exec(const TCHAR* Cmd) const;
	UTestDriverSubsystem* GetDriver() const;

	EPhase Phase = EPhase::Idle;
	FPerfWalkOptions Opts;
	TArray<FPerfWalkStep> Steps;
	int32 StepIndex = 0;
	float Elapsed = 0.f;          // per-step accumulator (DeltaTime, deterministic)
	bool bStepInit = false;

	// scratch for the active step
	FVector StartPos = FVector::ZeroVector;
	FVector EndPos = FVector::ZeroVector;
	float StartYaw = 0.f;
};
