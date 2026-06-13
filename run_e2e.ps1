param(
    [string]$TestName = "HappyPath",
    [switch]$Headed,
    [int]$TimeoutMinutes = 0
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

# Default timeout: 60 min when running the full Regression suite (9 tests * ~2-5 min),
# 20 min otherwise.
if ($TimeoutMinutes -le 0) {
    if ($TestName -eq 'Regression') { $TimeoutMinutes = 60 } else { $TimeoutMinutes = 20 }
}

# Clean previous logs
Remove-Item $LogFile -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stdout.log" -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -ErrorAction SilentlyContinue

Write-Host "Running E2E test: $TestPath"
Write-Host "Log: $LogFile"
Write-Host ""

$stdoutLog = "$ProjectRoot\Saved\Logs\E2ETest_stdout.log"
$argList = @(
    "`"$UProject`""
    "-ExecCmds=`"Automation RunTests $TestPath; Quit`""
    "-unattended"
    "-nopause"
    "-nosplash"
    "-abslog=`"$LogFile`""
)
# Default: NullRHI for fast headless runs. Pass -Headed to render so screenshots
# are captured (NullRHI produces blank/zero-byte screenshots).
if (-not $Headed) {
    $argList += "-NullRHI"
}
$proc = Start-Process -FilePath $UECmd -ArgumentList $argList -RedirectStandardOutput $stdoutLog -RedirectStandardError "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -PassThru

# Watchdog: a hung editor must not block forever or leave an orphan process
# (a screenshot stall once kept UnrealEditor-Cmd alive for 2.5 hours).
if (-not $proc.WaitForExit($TimeoutMinutes * 60 * 1000)) {
    $proc.Kill()
    Write-Host "FAIL - $TestPath (timed out after $TimeoutMinutes minutes; killed UnrealEditor-Cmd pid $($proc.Id))"
    exit 1
}

if (-not (Test-Path $LogFile)) {
    Write-Host "FAIL - No log file produced"
    exit 1
}

$log = Get-Content $LogFile -Raw

# Check for test result line
$resultMatch = [regex]::Match($log, 'Test Completed\. Result=\{(\w+)\} Name=\{[^}]*\} Path=\{[^}]*\}')

if ($resultMatch.Success) {
    $result = $resultMatch.Groups[1].Value
    if ($result -eq "Success") {
        Write-Host "PASS - $TestPath"
        # Show step count
        $steps = ([regex]::Matches($log, 'TestDriver::Status')).Count
        Write-Host "  ($steps test steps executed)"
        exit 0
    } else {
        Write-Host "FAIL - $TestPath (Result: $result)"
    }
} else {
    Write-Host "FAIL - $TestPath (no test result found in log)"
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
