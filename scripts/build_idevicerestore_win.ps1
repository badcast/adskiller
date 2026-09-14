# Скрипт подготовки и копирования официальных утилит idevicerestore для Windows
# Запускается на сборочной машине Windows (CI / Local build)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir
$DestDir = Join-Path $RootDir "3rdParty\apple"
$BuildAppleDir = Join-Path $RootDir "build\apple"

Write-Host "=== Подготовка idevicerestore для Windows из официальных источников ==="
New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
New-Item -ItemType Directory -Force -Path $BuildAppleDir | Out-Null

# Источник: официальный порт libimobiledevice / idevicerestore для Windows x64
$ZipUrl = "https://github.com/libimobiledevice-win32/imobiledevice-net/releases/download/v1.3.17/libimobiledevice.x64.zip"
$ZipPath = Join-Path $RootDir "build\idevicerestore_win.zip"

try {
    Write-Host "Загрузка официального пакета Windows x64..."
    Invoke-WebRequest -Uri $ZipUrl -OutFile $ZipPath -UseBasicParsing
    
    Write-Host "Распаковка и копирование утилит в $DestDir..."
    Expand-Archive -Path $ZipPath -DestinationPath $DestDir -Force
    Expand-Archive -Path $ZipPath -DestinationPath $BuildAppleDir -Force
    Remove-Item $ZipPath -Force

    Write-Host "=== Успешно! Файлы idevicerestore скопированы в 3rdParty/apple и build/apple ==="
    Get-ChildItem $DestDir | Select-Object Name, Length
} catch {
    Write-Warning "Ошибка автоматической загрузки: $_"
    exit 1
}
