param(
    [string]$TestName = "HappyPath",
    [switch]$Headed,
    [int]$TimeoutMinutes = 0,
    # Optional console commands (e.g. cvars) run at startup before the test, for
    # perf A/B experiments. UE -ExecCmds separates commands by COMMA, so separate
    # multiple cvars with commas. Example: -ExtraExec "r.Streaming.PoolSize 3000,r.Shadow.Virtual.Enable 0"
    # To restore slow-motion step pacing for headed review runs: -ExtraExec "e2e.StepDelay 0.3"
    [string]$ExtraExec = "",
    # Capture an Unreal Insights .utrace (cpu+gpu+frame) to Saved/Profiling/walk_trace.utrace
    # for offline analysis of render-thread / GPU / Lumen costs. Implies rendering.
    [switch]$Trace,
    # After the run, diff E2E_* screenshots against Tests/E2EGoldens and build the
    # HTML gallery (scripts/e2e_report.py). Meaningful with -Headed only.
    [switch]$Report
)

$ProjectRoot = $PSScriptRoot
$LogFile = "$ProjectRoot\Saved\Logs\E2ETest.log"
$UECmd = "C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$UProject = "$ProjectRoot\weirdplace2.uproject"

# Resolve subgroup: if caller already named Regression/Diagnostic (whole suite or a
# specific test under it), use as-is. Otherwise default to the Regression subgroup
# so existing -TestName HappyPath invocations keep working.
if ($TestName -match '^(Regression|Diagnostic)(\.|$)') {
    $TestPath = "Weirdplace2.E2E.Level1.$TestName"
} else {
    $TestPath = "Weirdplace2.E2E.Level1.Regression.$TestName"
}

# Default timeout: 90 min when running the full Regression suite (22 tests * ~2-5 min),
# 20 min otherwise.
if ($TimeoutMinutes -le 0) {
    if ($TestName -eq 'Regression') { $TimeoutMinutes = 90 } else { $TimeoutMinutes = 20 }
}

# Clean previous logs
Remove-Item $LogFile -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stdout.log" -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -ErrorAction SilentlyContinue

Write-Host "Running E2E test: $TestPath"
Write-Host "Log: $LogFile"
Write-Host ""

$stdoutLog = "$ProjectRoot\Saved\Logs\E2ETest_stdout.log"
$execCmds = if ($ExtraExec) { "$ExtraExec,Automation RunTests $TestPath; Quit" } else { "Automation RunTests $TestPath; Quit" }
$argList = @(
    "`"$UProject`""
    "-ExecCmds=`"$execCmds`""
    "-unattended"
    "-nopause"
    "-nosplash"
    # Fab is enabled in the .uproject (its Megascans base materials are used in the
    # level), but its browser tab auto-restores at startup and CreateBrowserWindow
    # returns null under NullRHI -> FFabBrowser::OpenTab asserts. Headless never needs
    # Fab, so disable it here to keep automation crash-free regardless of saved layout.
    "-DisablePlugins=Fab"
    "-abslog=`"$LogFile`""
)
if ($Trace) {
    $traceFile = "$ProjectRoot\Saved\Profiling\walk_trace.utrace"
    Remove-Item $traceFile -ErrorAction SilentlyContinue
    $argList += "-trace=default,gpu,counters"
    $argList += "-statnamedevents"
    $argList += "-tracefile=`"$traceFile`""
    Write-Host "Trace -> $traceFile"
}
# Default: NullRHI for fast headless runs. Pass -Headed to render so screenshots
# are captured (NullRHI produces blank/zero-byte screenshots).
if (-not $Headed) {
    $argList += "-NullRHI"
}
$proc = Start-Process -FilePath $UECmd -ArgumentList $argList -RedirectStandardOutput $stdoutLog -RedirectStandardError "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -PassThru

# Watchdog: a hung editor must not block forever or leave an orphan process
# (a screenshot stall once kept UnrealEditor-Cmd alive for 2.5 hours).
# On timeout we kill the process but fall through to the log-parsing block so
# the output names which tests started but never completed.
$timedOut = $false
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    $timedOut = $true
    Write-Host "Watchdog: killed UnrealEditor-Cmd pid $($proc.Id) after $TimeoutMinutes minutes; parsing log..."
    # Give the log file a moment to flush before reading it.
    Start-Sleep -Seconds 2
}

if (-not (Test-Path $LogFile)) {
    Write-Host "FAIL - No log file produced"
    exit 1
}

$log = Get-Content $LogFile -Raw

# A mid-suite crash exits the editor after only some tests have run, so the FIRST
# "Test Completed" line is NOT a verdict for the whole group. Validate the entire
# run instead:
#   * every started test also completed (a crash/hang leaves Started > Completed)
#   * every completed result is Success
#   * no engine crash / fatal markers in the log
$started   = [regex]::Matches($log, 'Test Started\. Name=\{([^}]*)\}')
$completed = [regex]::Matches($log, 'Test Completed\. Result=\{(\w+)\} Name=\{([^}]*)\}')
$crashMatch = [regex]::Match($log, 'StaticShutdownAfterError|Fatal error|=== Critical error|Assertion failed')

$failedTests = @()
foreach ($m in $completed) {
    if ($m.Groups[1].Value -ne 'Success') {
        $failedTests += "$($m.Groups[2].Value) (Result: $($m.Groups[1].Value))"
    }
}
$incomplete = $started.Count - $completed.Count

if ((-not $timedOut) -and ($started.Count -gt 0) -and ($failedTests.Count -eq 0) -and ($incomplete -eq 0) -and (-not $crashMatch.Success)) {
    Write-Host "PASS - $TestPath"
    Write-Host "  ($($completed.Count) test(s) passed)"
    $steps = ([regex]::Matches($log, 'TestDriver::Status')).Count
    Write-Host "  ($steps test steps executed)"
    if ($Report) {
        Write-Host ""
        Write-Host "Screenshot report (scripts/e2e_report.py):"
        python "$ProjectRoot\scripts\e2e_report.py"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "REPORT FAIL - screenshot diffs above; tests themselves passed"
            exit 1
        }
    }
    exit 0
}

Write-Host "FAIL - $TestPath"
if ($timedOut) {
    Write-Host "  Timed out after $TimeoutMinutes minutes."
}
if ($started.Count -eq 0) {
    Write-Host "  No tests ran (no 'Test Started' in log)."
}
if ($incomplete -gt 0) {
    $startedNames = $started | ForEach-Object { $_.Groups[1].Value }
    $completedNames = $completed | ForEach-Object { $_.Groups[2].Value }
    $missing = @($startedNames | Where-Object { $completedNames -notcontains $_ })
    Write-Host "  $incomplete test(s) started but never completed (crash/hang?): $($missing -join ', ')"
}
if ($crashMatch.Success) {
    Write-Host "  Engine crash/fatal marker detected in log: '$($crashMatch.Value)'"
}
if ($failedTests.Count -gt 0) {
    Write-Host "  Failed: $($failedTests -join ', ')"
}

# Print test errors from the log (skip the "Test Completed" summary line)
$errors = [regex]::Matches($log, '(?m)^.*LogAutomationController: Error:(?!.*Test Completed).*$')
if ($errors.Count -gt 0) {
    Write-Host ""
    Write-Host "Errors:"
    foreach ($e in $errors) {
        # Strip timestamp prefix for readability
        $line = $e.Value -replace '^\[.*?\]\[.*?\]', ''
        Write-Host "  $line"
    }
}

# Print any AddError lines from our test code
$testErrors = [regex]::Matches($log, '(?m)^.*Error.*TestDriver.*$')
if ($testErrors.Count -gt 0) {
    Write-Host ""
    Write-Host "Test driver errors:"
    foreach ($e in $testErrors) {
        $line = $e.Value -replace '^\[.*?\]\[.*?\]', ''
        Write-Host "  $line"
    }
}

exit 1
