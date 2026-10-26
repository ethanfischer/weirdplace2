---
name: itch-deploy
description: Package the latest merged master build for Windows and push it to itch.io (garglesmcgee/weird-place:windows). Use when the user asks to deploy, publish, or push a build to itch.
---

`powershell -ExecutionPolicy Bypass -File scripts/itch_deploy.ps1` (run in the background: full build + cook takes 20-60 min).

- Refuses unless the working copy is `master`, clean, and equal to `origin/master`, so only the latest merge goes up. Don't switch the user's branch to satisfy it without asking.
- `-DryRun` packages and runs `butler push --dry-run` (publishes nothing). `-Config Shipping` for a Shipping build (default Development, which is what itch has had).
- Uses this PC's butler login. If butler isn't logged in, the user runs `! butler login`.
- Pushing publishes to the public itch page: confirm with the user before a real (non-dry-run) push unless they asked for one.
