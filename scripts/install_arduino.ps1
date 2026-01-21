#!/usr/bin/env pwsh
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Command($Name) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        Write-Host "Missing '$Name'. Install it and try again." -ForegroundColor Red
        exit 1
    }
}

function Find-ArduinoCli {
    $cmd = Get-Command "arduino-cli" -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $candidates = @(
        (Join-Path $env:ProgramFiles "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"),
        (Join-Path $env:ProgramFiles "Arduino CLI\arduino-cli.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "Arduino CLI\arduino-cli.exe"),
        (Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Links\arduino-cli.exe")
    ) | Where-Object { $_ -and (Test-Path $_) }

    if ($candidates) { return ($candidates | Select-Object -First 1) }
    return $null
}

Require-Command "winget"

Write-Host "Installing Arduino IDE..." -ForegroundColor Cyan
winget install --id ArduinoSA.IDE -e --source winget

Write-Host "Installing Arduino AVR core (brings avrdude)..." -ForegroundColor Cyan
$arduinoCli = Find-ArduinoCli
if (-not $arduinoCli) {
    Write-Host "arduino-cli not found. Restart PowerShell or check the install path." -ForegroundColor Red
    exit 1
}
Write-Host ("Using arduino-cli at: {0}" -f $arduinoCli) -ForegroundColor Yellow
Start-Process -FilePath $arduinoCli -ArgumentList @("core", "update-index") -Wait -NoNewWindow
Start-Process -FilePath $arduinoCli -ArgumentList @("core", "install", "arduino:avr") -Wait -NoNewWindow

Write-Host ""
Write-Host "Done. Restart PowerShell/terminal to refresh PATH." -ForegroundColor Green
