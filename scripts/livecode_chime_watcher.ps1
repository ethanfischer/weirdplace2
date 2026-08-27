# Plays a chime whenever Live Coding finishes (success = Asterisk, fail = Hand).
# UE's built-in compile jingle has been broken for a while; this replaces it.
# Spawned by launch_editor.ps1; exits when the editor does. Safe to run manually.
$log = Join-Path (Split-Path $PSScriptRoot) 'Saved\Logs\weirdplace2.log'
$offset = (Get-Item $log -ErrorAction SilentlyContinue).Length
if ($null -eq $offset) { $offset = 0 }

# Single-instance guard: bail if another watcher is already running.
$mutex = New-Object System.Threading.Mutex($false, 'weirdplace2_livecode_chime')
if (-not $mutex.WaitOne(0)) { exit 0 }

while (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
    Start-Sleep -Milliseconds 1500
    $item = Get-Item $log -ErrorAction SilentlyContinue
    if ($null -eq $item) { continue }
    if ($item.Length -lt $offset) { $offset = 0 }   # log rotated on editor restart
    if ($item.Length -eq $offset) { continue }
    $fs = [System.IO.File]::Open($log, 'Open', 'Read', 'ReadWrite')
    $fs.Seek($offset, 'Begin') | Out-Null
    $new = (New-Object System.IO.StreamReader($fs)).ReadToEnd()
    $offset = $fs.Position
    $fs.Close()
    if ($new -match 'Live coding succeeded') { [System.Media.SystemSounds]::Asterisk.Play() }
    elseif ($new -match 'Live coding failed|Live coding aborted') { [System.Media.SystemSounds]::Hand.Play() }
}
