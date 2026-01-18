# Project Notes

## C++ Compilation

**Live Coding (Ctrl+Alt+F11 in UE Editor)** - use for:
- `.cpp` implementation changes (function body edits)

**Full Restart Required** - close UE, rebuild from Rider:
- Adding/removing `UPROPERTY` or `UFUNCTION`
- Changing function signatures in headers
- Adding new classes or files
- Changing class inheritance
- Modifying `.Build.cs`

Always tell Ethan which type of rebuild is needed after making changes.

## Windows Git Performance

Live Coding uses `git status` for adaptive unity builds. Without fsmonitor, this takes 60+ seconds on large repos. Enable these settings for Windows git:

```cmd
git config core.fsmonitor true
git config core.untrackedcache true
```

This brings `git status` from ~65 seconds down to ~70ms by using a background daemon to track changes instead of scanning all files.

**Important:** Run git commands from Windows (not WSL) to use the daemon. From WSL you can use:
```bash
git.exe status   # Calls Windows git, uses daemon, fast
git status       # Calls WSL git, no daemon, slow
```

**WSL + Windows Git Conflict:** Both share the same `.git` directory. Running WSL git can invalidate the Windows git cache, causing a slow re-index. To avoid this, use Windows git exclusively when possible.

**Lazygit:** Install Windows lazygit (`winget install lazygit`) and run from a Windows Terminal/PowerShell tab. Windows console apps don't work properly when invoked from WSL due to TTY issues.

## macOS / Xcode Compatibility

This project uses Unreal Engine 5.4. On macOS 26 with Xcode 26.x, edit the Apple SDK config to allow the newer Xcode version:

**File:** `/Users/ethan/Epic Games/UE_5.4/UE_5.4/Engine/Config/Apple/Apple_SDK.json`

Change `MaxVersion` from `"16.9.0"` to `"26.9.0"` (or higher).

Reference: https://forums.unrealengine.com/t/conflict-running-unreal-engine-on-mac-os/2657924/7
