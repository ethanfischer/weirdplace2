#pragma once

#include "HAL/IConsoleManager.h"

// Live-tunable constants. Instead of hardcoding a gameplay constant (every tweak =
// recompile) or burying it in a level-instance property (drifts from code), declare:
//
//     WP_TUNABLE_FLOAT(GHeadlightIntensity, "weird.Headlight.Intensity", 45000.f,
//         "Car headlight intensity in lumens.");
//
// then read the static in Tick/wherever. Tweak live from the console or terminal:
//
//     uq cvar weird.Headlight.Intensity 60000
//
// `weird.Tunables` (or `uq cvar --dump`) lists every weird.* value; '*' marks ones
// changed via console this session — bake those back into the Default here when
// dialed in. In-game (non-Shipping): Pause menu → Tunables browses and tweaks
// all weird.* cvars. Name new tunables "weird.<System>.<Name>" ("wp." is taken: World Partition).
// NOTE: cvar registration happens in static initializers — a NEW tunable needs a
// full editor restart, not Live Coding. Tweaks to existing ones are always live.

// Baked defaults by cvar name, so the in-game Tunables page can reset a system
// back to code values. IConsoleVariable doesn't remember its initial value.
struct FTunableDefaults
{
	static bool Register(const TCHAR* Name, const FString& Default);
	static const FString* Find(const FString& Name);
};

#define WP_TUNABLE_FLOAT(Var, Name, Default, Help) \
	static float Var = Default; \
	static FAutoConsoleVariableRef CVarRef_##Var(TEXT(Name), Var, TEXT(Help)); \
	static const bool TunableDefault_##Var = FTunableDefaults::Register(TEXT(Name), LexToString(Var));

#define WP_TUNABLE_INT(Var, Name, Default, Help) \
	static int32 Var = Default; \
	static FAutoConsoleVariableRef CVarRef_##Var(TEXT(Name), Var, TEXT(Help)); \
	static const bool TunableDefault_##Var = FTunableDefaults::Register(TEXT(Name), LexToString(Var));

#define WP_TUNABLE_BOOL(Var, Name, Default, Help) \
	static bool Var = Default; \
	static FAutoConsoleVariableRef CVarRef_##Var(TEXT(Name), Var, TEXT(Help)); \
	static const bool TunableDefault_##Var = FTunableDefaults::Register(TEXT(Name), LexToString(Var));
