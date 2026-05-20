#Requires -Version 5.1
<#
.SYNOPSIS
  Sync the Linux package to the Steam Deck over SSH using rsync.

.PARAMETER DeckHost
  Username@host (e.g. deck@192.168.1.42). Defaults to $env:DECK_HOST.

.PARAMETER Source
  Local source directory. Defaults to Packaged/SteamDeck-Linux/Linux/.

.PARAMETER DestDir
  Remote destination directory. Defaults to ~/Games/weirdplace2.

.EXAMPLE
  $env:DECK_HOST = 'deck@192.168.1.42'
  ./scripts/push_to_deck.ps1
#>

param(
    [string]$DeckHost = $env:DECK_HOST,
    [string]$Source = (Join-Path $PSScriptRoot '..\Packaged\SteamDeck-Linux\Linux'),
    [string]$DestDir = '~/Games/weirdplace2'
)

$ErrorActionPreference = 'Stop'

if (-not $DeckHost) {
    throw "DeckHost not set. Pass -DeckHost user@ip or set `$env:DECK_HOST."
}

$rsync = 'C:\msys64\usr\bin\rsync.exe'
if (-not (Test-Path $rsync)) {
    throw "rsync not found at $rsync. Install via: winget install MSYS2.MSYS2 && C:\msys64\usr\bin\bash -lc 'pacman -S --noconfirm rsync'"
}

$Source = (Resolve-Path $Source).Path
if (-not (Test-Path $Source)) {
    throw "Source not found: $Source"
}

# MSYS rsync treats "C:\..." as a remote host (C = hostname). Translate to /c/...
$src = $Source.TrimEnd('\','/') -replace '\\','/'
if ($src -match '^([A-Za-z]):(.*)$') {
    $src = '/' + $Matches[1].ToLower() + $Matches[2]
}
$src += '/'

# MSYS rsync needs MSYS ssh (stdio piping compatibility).
# The id_ed25519 key must also be present in C:\msys64\home\<user>\.ssh\.
$ssh = 'C:\msys64\usr\bin\ssh.exe'
if (-not (Test-Path $ssh)) { $ssh = (Get-Command ssh -ErrorAction Stop).Source }

Write-Host "Ensuring $DestDir exists on Deck..." -ForegroundColor Cyan
& $ssh $DeckHost "mkdir -p $DestDir"
if ($LASTEXITCODE -ne 0) { throw "ssh mkdir failed with exit code $LASTEXITCODE" }

Write-Host "Syncing $src -> ${DeckHost}:$DestDir" -ForegroundColor Cyan
& $rsync -avzP --delete -e $ssh $src "${DeckHost}:$DestDir/"

if ($LASTEXITCODE -ne 0) {
    throw "rsync failed with exit code $LASTEXITCODE"
}

Write-Host "Done. On the Deck, launch: $DestDir/weirdplace2.sh" -ForegroundColor Green
