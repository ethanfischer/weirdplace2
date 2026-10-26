param(
    # Development matches what's been on itch so far (console + SkipTo available).
    [ValidateSet('Development', 'Shipping')]
    [string]$Config = 'Development',
    # Package and run butler with --dry-run: shows what would upload, publishes nothing.
    [switch]$DryRun
)

# Package the merged master build for Win64 and push it to itch.io with butler.
#   powershell -ExecutionPolicy Bypass -File scripts/itch_deploy.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/itch_deploy.ps1 -DryRun
# Packages the working copy, so it must be master, clean, and equal to origin/master:
# what goes up is exactly the latest merge. Uses this PC's butler login (`butler login`).

$ErrorActionPreference = 'Stop'

$ItchTarget = 'garglesmcgee/weird-place:windows'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$UProject = Join-Path $RepoRoot 'weirdplace2.uproject'
$RunUAT = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat'
$StagedDir = Join-Path $RepoRoot 'Saved\StagedBuilds\Windows'

function Fail([string]$Message) {
    Write-Host "FAIL - $Message"
    exit 1
}

function Invoke-Git([string[]]$GitArgs) {
    $out = & git -C $RepoRoot @GitArgs
    if ($LASTEXITCODE -ne 0) { Fail "git $($GitArgs -join ' ') exited $LASTEXITCODE" }
    return $out
}

# --- Preflight: only ever publish the latest merge ---
if (-not (Get-Command butler -ErrorAction SilentlyContinue)) {
    Fail 'butler not on PATH (winget install butler; butler login)'
}
$Branch = Invoke-Git @('rev-parse', '--abbrev-ref', 'HEAD')
if ($Branch -ne 'master') { Fail "on branch '$Branch', not master" }
if (Invoke-Git @('status', '--porcelain')) { Fail 'working tree has uncommitted changes' }
Invoke-Git @('fetch', 'origin', 'master', '--quiet') | Out-Null
$Head = Invoke-Git @('rev-parse', 'HEAD')
$Remote = Invoke-Git @('rev-parse', 'origin/master')
if ($Head -ne $Remote) { Fail "HEAD $($Head.Substring(0, 8)) != origin/master $($Remote.Substring(0, 8)) (git pull first)" }
$Version = $Head.Substring(0, 8)

# --- Package (stage only: the staged folder is the itch layout, no archive copy) ---
Write-Host "Packaging $Version ($Config)..."
& $RunUAT BuildCookRun `
    -project="$UProject" `
    -noP4 -platform=Win64 -clientconfig="$Config" `
    -build -cook -allmaps -stage -pak `
    -nodebuginfo -utf8output -unattended
if ($LASTEXITCODE -ne 0) { Fail "BuildCookRun exited $LASTEXITCODE" }
if (-not (Test-Path (Join-Path $StagedDir 'weirdplace2.exe'))) { Fail "no weirdplace2.exe in $StagedDir" }

# --- Push ---
$ButlerArgs = @('push', $StagedDir, $ItchTarget, '--userversion', $Version)
if ($DryRun) { $ButlerArgs += '--dry-run' }
& butler @ButlerArgs
if ($LASTEXITCODE -ne 0) { Fail "butler push exited $LASTEXITCODE" }

Write-Host "PASS - $(if ($DryRun) { 'dry run of' } else { 'pushed' }) $Version to $ItchTarget"
