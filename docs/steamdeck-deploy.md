# Steam Deck Deploy

Linux Development builds run natively on the Deck (no Proton). Push over SSH from PC, launch via Steam.

## One-time setup (already done on this machine)

- `LINUX_MULTIARCH_ROOT` env var → `C:\UnrealToolchains\v22_clang-16.0.6-centos7\` (Machine scope, but not always inherited by tool shells — set inline if a build fails with "Platform Linux is not a valid platform to build").
- MSYS2 at `C:\msys64\` with `rsync` + `openssh` packages.
- SSH key at `C:\Users\ethan\.ssh\id_ed25519` AND mirrored to `C:\msys64\home\ethan\.ssh\`. MSYS rsync uses MSYS's home, not Windows OpenSSH's — if a password prompt appears, the key isn't where MSYS ssh expects.
- Deck has sshd enabled and the pubkey in `~/.ssh/authorized_keys`.
- Deck IP varies per network. Currently `192.168.0.15`. Verify with the user. Deck sleeps aggressively; first SSH after sleep may time out.

## Build

From PowerShell so env vars inherit:

```powershell
$env:LINUX_MULTIARCH_ROOT = 'C:\UnrealToolchains\v22_clang-16.0.6-centos7\'
& "C:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun `
  -project="C:/Users/ethan/repos/weirdplace2/weirdplace2.uproject" `
  -noP4 -platform=Linux -clientconfig=Development `
  -cook -allmaps -build -stage -pak -archive `
  -archivedirectory="C:/Users/ethan/repos/weirdplace2/Packaged/SteamDeck-Linux" `
  -nodebuginfo -utf8output -unattended
```

Pick the right combination of `-build` / `-cook` / `-stage` based on what changed:

| Change                            | Flags                                | Time      |
| --------------------------------- | ------------------------------------ | --------- |
| New `UPROPERTY` / class / layout  | `-cook -allmaps -build -stage`       | ~10-20min |
| `.cpp`-only                       | `-build -skipcook -stage`            | ~3min     |
| `.ini` / config only              | `-skipbuild -skipcook -stage`        | ~90s      |

**Don't skip cook after layout changes.** Cooked Blueprint subclasses serialize property indices; reusing stale cooked data after a `UPROPERTY` add produces `ObjectSerializationError: Bad export index` and the game hangs on Steam's loading screen.

Output lands in `Packaged/SteamDeck-Linux/Linux/`. Don't use Shipping config for in-progress work — it strips logging and you lose ability to diagnose.

## Push

```powershell
./scripts/push_to_deck.ps1 -DeckHost deck@192.168.0.15
```

Uses MSYS rsync + MSYS ssh, syncs to `~/Games/weirdplace2/`. Passwordless via SSH key. Delta transfer: seconds for incremental, ~minutes for full.

## Launch

Must be launched **through Steam** on the Deck (Add as Non-Steam Game once, then launch from the library). Running `weirdplace2.sh` directly bypasses Steam Input → SDL doesn't enumerate the Deck's controller → no gamepad input works at all.

## Reading Deck logs

Passwordless SSH lets you grab logs directly:

```bash
scp -o BatchMode=yes deck@192.168.0.15:/home/deck/Games/weirdplace2/weirdplace2/Saved/Logs/weirdplace2.log Saved/DeckLogs/current.log
```

Then grep locally. Crash dumps are at `Saved/Crashes/crashinfo-*/weirdplace2.log` on the Deck.

## Deck device profile

`Config/DefaultDeviceProfiles.ini` defines the `SteamDeck` profile (sg.* cvars, t.MaxFPS). It's selected by `Source/weirdplace2DeviceProfileSelector/`, a custom module that reads `/etc/os-release` for `ID=steamos`/`ID=holo`. Wired by `Config/Linux/LinuxEngine.ini`. UE 5.4 has zero built-in Steam Deck detection — this is why the custom selector exists.

Gotchas:
- `sg.ResolutionQuality` is a **percentage (1-100)**, not a 0-3 quality level like the other `sg.*` groups. 75 = 75% screen percentage.
- `AMyCharacter::BeginPlay` re-applies scalability (`Scalability::SetQualityLevels(..., true)`) to work around a Lumen warmup bug where DeviceProfile cvars apply before Lumen is ready, causing the first frame to look wrong. Don't remove that.
