param(
    [string]$TestName = "HappyPath"
)

$ProjectRoot = $PSScriptRoot
$LogFile = "$ProjectRoot\Saved\Logs\E2ETest.log"
$UECmd = "C:\Program Files\Epic Games\UE_5.4\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$UProject = "$ProjectRoot\weirdplace2.uproject"
$TestPath = "Weirdplace2.E2E.Level1.$TestName"

# Clean previous logs
Remove-Item $LogFile -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stdout.log" -ErrorAction SilentlyContinue
Remove-Item "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -ErrorAction SilentlyContinue

Write-Host "Running E2E test: $TestPath"
Write-Host "Log: $LogFile"
Write-Host ""

$stdoutLog = "$ProjectRoot\Saved\Logs\E2ETest_stdout.log"
$proc = Start-Process -FilePath $UECmd -ArgumentList @(
    "`"$UProject`""
    "-ExecCmds=`"Automation RunTests $TestPath; Quit`""
    "-unattended"
    "-nopause"
    "-nosplash"
    "-NullRHI"
    "-abslog=`"$LogFile`""
) -RedirectStandardOutput $stdoutLog -RedirectStandardError "$ProjectRoot\Saved\Logs\E2ETest_stderr.log" -PassThru -Wait

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
