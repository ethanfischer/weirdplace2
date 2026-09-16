#include "AudioMixSubsystem.h"
#include "Tunable.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Volume per SoundClass. Class volumes chain down the tree (SC_Master *
// SC_SFX * SC_Doors), so Master/Music/Sfx act as group faders.
// ---------------------------------------------------------------------------
WP_TUNABLE_FLOAT(GAudioMaster,      "weird.Audio.Master",      1.0f, "SC_Master volume: everything.");
WP_TUNABLE_FLOAT(GAudioMusic,       "weird.Audio.Music",       1.0f, "SC_Music volume.");
WP_TUNABLE_FLOAT(GAudioSfx,         "weird.Audio.Sfx",         1.0f, "SC_SFX volume: every non-music sound.");
WP_TUNABLE_FLOAT(GAudioAmbient,     "weird.Audio.Ambient",     1.0f, "SC_Ambient volume: wind, waterfall, room tones, chord hum.");
WP_TUNABLE_FLOAT(GAudioFootsteps,   "weird.Audio.Footsteps",   1.0f, "SC_Footsteps volume.");
WP_TUNABLE_FLOAT(GAudioDoors,       "weird.Audio.Doors",       1.0f, "SC_Doors volume: open, locked, key, latch, glass stomp.");
WP_TUNABLE_FLOAT(GAudioMenu,        "weird.Audio.Menu",        1.0f, "SC_Menu volume: pause menu open/close/select.");
WP_TUNABLE_FLOAT(GAudioItemCollect, "weird.Audio.ItemCollect", 1.0f, "SC_ItemCollect volume: pickup stingers.");
WP_TUNABLE_FLOAT(GAudioPhone,       "weird.Audio.Phone",       1.0f, "SC_Phone volume: handset sounds and announcement voice.");
WP_TUNABLE_FLOAT(GAudioDialogue,    "weird.Audio.Dialogue",    1.0f, "SC_Dialogue volume: text blips, low voice, passcode.");
WP_TUNABLE_FLOAT(GAudioBladder,     "weird.Audio.Bladder",     1.0f, "SC_Bladder volume.");
WP_TUNABLE_FLOAT(GAudioGaze,        "weird.Audio.Gaze",        1.0f, "SC_Gaze volume: gaze hum and pluck.");

namespace
{
	struct FClassFader
	{
		const float* Volume;
		const TCHAR* ClassPath;
	};

	const FClassFader GFaders[] =
	{
		{ &GAudioMaster,      TEXT("/Game/Sounds/Classes/SC_Master.SC_Master") },
		{ &GAudioMusic,       TEXT("/Game/Sounds/Classes/SC_Music.SC_Music") },
		{ &GAudioSfx,         TEXT("/Game/Sounds/Classes/SC_SFX.SC_SFX") },
		{ &GAudioAmbient,     TEXT("/Game/Sounds/SC_Ambient.SC_Ambient") },
		{ &GAudioFootsteps,   TEXT("/Game/Sounds/Classes/SC_Footsteps.SC_Footsteps") },
		{ &GAudioDoors,       TEXT("/Game/Sounds/Classes/SC_Doors.SC_Doors") },
		{ &GAudioMenu,        TEXT("/Game/Sounds/Classes/SC_Menu.SC_Menu") },
		{ &GAudioItemCollect, TEXT("/Game/Sounds/Classes/SC_ItemCollect.SC_ItemCollect") },
		{ &GAudioPhone,       TEXT("/Game/Sounds/Classes/SC_Phone.SC_Phone") },
		{ &GAudioDialogue,    TEXT("/Game/Sounds/Classes/SC_Dialogue.SC_Dialogue") },
		{ &GAudioBladder,     TEXT("/Game/Sounds/Classes/SC_Bladder.SC_Bladder") },
		{ &GAudioGaze,        TEXT("/Game/Sounds/Classes/SC_Gaze.SC_Gaze") },
	};
}

bool UAudioMixSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAudioMixSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// Transient mix: it only exists to carry the class overrides below.
	Mix = NewObject<USoundMix>(this, TEXT("TunablesSoundMix"));
	UGameplayStatics::PushSoundMixModifier(&InWorld, Mix);
	Apply();

	// Any cvar change (menu, console, uq) re-pushes every fader. Cheap: 12 overrides.
	SinkHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(
		FConsoleCommandDelegate::CreateUObject(this, &UAudioMixSubsystem::Apply));
}

void UAudioMixSubsystem::Deinitialize()
{
	IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(SinkHandle);
	if (Mix)
	{
		// The mix was pushed on the world's audio device; pop it so the
		// overrides don't outlive this subsystem.
		UGameplayStatics::PopSoundMixModifier(GetWorld(), Mix);
		Mix = nullptr;
	}
	Super::Deinitialize();
}

void UAudioMixSubsystem::Apply()
{
	UWorld* World = GetWorld();
	if (!World || !Mix)
	{
		return;
	}
	for (const FClassFader& F : GFaders)
	{
		USoundClass* Class = LoadObject<USoundClass>(nullptr, F.ClassPath);
		if (!Class)
		{
			UE_LOG(LogTemp, Error, TEXT("AudioMixSubsystem: SoundClass %s not found"), F.ClassPath);
			continue;
		}
		// bApplyToChildren: mix adjusters run after the class hierarchy is
		// flattened, so a parent override only reaches children this way. Each
		// child ends up with the product of every ancestor's fader.
		UGameplayStatics::SetSoundMixClassOverride(World, Mix, Class, *F.Volume, /*Pitch*/1.0f, /*FadeIn*/0.1f, /*bApplyToChildren*/true);
	}
}
