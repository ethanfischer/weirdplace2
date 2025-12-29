# Project Notes

## macOS / Xcode Compatibility

This project uses Unreal Engine 5.4. On macOS 26 with Xcode 26.x, edit the Apple SDK config to allow the newer Xcode version:

**File:** `/Users/ethan/Epic Games/UE_5.4/UE_5.4/Engine/Config/Apple/Apple_SDK.json`

Change `MaxVersion` from `"16.9.0"` to `"26.9.0"` (or higher).

Reference: https://forums.unrealengine.com/t/conflict-running-unreal-engine-on-mac-os/2657924/7
