<#
.SYNOPSIS
    Elevated half of the heap-trace pipeline.

    Sequence:
      1. wpr -heaptracingconfig UnrealEditor.exe enable
      2. Write setup_done flag
      3. Wait for ue_ready flag (Claude relaunches UE non-elevated and writes it)
      4. Warm-up wait
      5. wpr -start Heap -filemode
      6. Capture wait
      7. wpr -stop <etl>
      8. wpr -heaptracingconfig UnrealEditor.exe disable
      9. Write done flag (contains result info)
#>

param(
    [int]$WarmupSec = 180,
    [int]$CaptureSec = 300,
    [int]$UeReadyTimeoutSec = 600
)

# NOTE: do NOT set $ErrorActionPreference = "Stop" — it triggers
# NativeCommandError when native exes write to stderr. We check $LASTEXITCODE
# explicitly after each native call.

$OutDir = "C:\Users\ethan\repos\weirdplace2\Saved\Profiling\Heap"
if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

$SetupFlag = Join-Path $OutDir "setup_done.flag"
$UeReadyFlag = Join-Path $OutDir "ue_ready.flag"
$DoneFlag = Join-Path $OutDir "done.flag"
$LogFile = Join-Path $OutDir "orchestrate.log"

Remove-Item $SetupFlag,$UeReadyFlag,$DoneFlag -ErrorAction SilentlyContinue

function Log {
    param([string]$Msg)
    $line = "[" + (Get-Date -Format "HH:mm:ss") + "] $Msg"
    Write-Host $line
    Add-Content -Path $LogFile -Value $line -Encoding utf8
}

function Fail {
    param([string]$Reason)
    Log "FAIL: $Reason"
    "FAILED: $Reason" | Out-File $DoneFlag -Encoding utf8
    # Best-effort teardown.
    wpr -heaptracingconfig UnrealEditor.exe disable | Out-Null
    Write-Host ""
    Write-Host "Press Enter to close (failed)..."
    [void][System.Console]::ReadLine()
    exit 1
}

Log "Orchestrator starting (admin). PID=$PID"

# Step 1: enable heap tracing.
Log "Step 1: wpr -heaptracingconfig UnrealEditor.exe enable"
wpr -heaptracingconfig UnrealEditor.exe enable
$rc = $LASTEXITCODE
Log "  wpr exit code: $rc"
if ($rc -ne 0) { Fail "wpr -heaptracingconfig on returned $rc" }

# Step 2: signal setup done.
"setup done at " + (Get-Date) | Out-File $SetupFlag -Encoding utf8
Log "Step 2: wrote $SetupFlag"

# Step 3: wait for UE relaunch.
Log "Step 3: waiting for $UeReadyFlag (timeout ${UeReadyTimeoutSec}s)"
$deadline = (Get-Date).AddSeconds($UeReadyTimeoutSec)
while (-not (Test-Path $UeReadyFlag)) {
    if ((Get-Date) -gt $deadline) { Fail "UE ready flag never appeared" }
    Start-Sleep -Seconds 5
}
Log "  ue_ready flag detected"

# Step 4: warmup.
Log "Step 4: warmup ${WarmupSec}s"
Start-Sleep -Seconds $WarmupSec

$proc = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if (-not $proc) { Fail "UE not running after warmup" }
Log "  UE PID: $($proc.Id)"

# Step 5: start trace.
Log "Step 5: wpr -start Heap -filemode"
wpr -start Heap -filemode
$rc = $LASTEXITCODE
Log "  wpr exit code: $rc"
if ($rc -ne 0) { Fail "wpr -start returned $rc" }

# Step 6: capture wait.
Log "Step 6: capturing for ${CaptureSec}s"
$elapsed = 0
while ($elapsed -lt $CaptureSec) {
    Start-Sleep -Seconds 30
    $elapsed += 30
    Log "  ${elapsed}/${CaptureSec}s captured"
}

# Step 7: stop trace.
$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$etl = Join-Path $OutDir "heap_$stamp.etl"
Log "Step 7: wpr -stop $etl"
wpr -stop $etl
$rc = $LASTEXITCODE
Log "  wpr exit code: $rc"
if ($rc -ne 0) { Fail "wpr -stop returned $rc" }

$etlSize = [math]::Round((Get-Item $etl).Length / 1MB, 1)
Log "  trace size: $etlSize MB"

# Step 8: teardown.
Log "Step 8: wpr -heaptracingconfig UnrealEditor.exe disable"
wpr -heaptracingconfig UnrealEditor.exe disable | Out-Null

# Step 9: write done flag.
@(
    "SUCCESS"
    "etl=$etl"
    "size_mb=$etlSize"
    "completed_at=" + (Get-Date)
) | Out-File $DoneFlag -Encoding utf8

Log "Done. Output: $etl ($etlSize MB)"
Log "You can close this window."
exit 0
