param(
    [Parameter(Mandatory = $true)]
    [string] $GameDir
)

$ErrorActionPreference = "Stop"

$sourceDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$releaseDir = Join-Path $sourceDir "release"

function Read-IniFile {
    param(
        [Parameter(Mandatory = $true)][string] $Path
    )

    $result = @{}
    $section = ""
    foreach ($line in Get-Content -LiteralPath $Path) {
        $trimmed = $line.Trim()
        if ($trimmed.Length -eq 0 -or $trimmed.StartsWith(";") -or $trimmed.StartsWith("#")) {
            continue
        }
        if ($trimmed.StartsWith("[") -and $trimmed.EndsWith("]")) {
            $section = $trimmed.Substring(1, $trimmed.Length - 2)
            if (-not $result.ContainsKey($section)) {
                $result[$section] = @{}
            }
            continue
        }
        $equals = $line.IndexOf("=")
        if ($equals -lt 0 -or $section.Length -eq 0) {
            continue
        }

        $key = $line.Substring(0, $equals).Trim()
        $value = $line.Substring($equals + 1).Trim()
        if (-not $result.ContainsKey($section)) {
            $result[$section] = @{}
        }
        $result[$section][$key] = $value
    }
    return $result
}

if (-not (Test-Path -LiteralPath $releaseDir)) {
    & (Join-Path $sourceDir "package_release.bat")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $GameDir)) {
    throw "GameDir does not exist: $GameDir"
}

$iniPath = Join-Path $GameDir "scripts\AC5Tools.ini"
$oldConfig = $null
if (Test-Path -LiteralPath $iniPath) {
    $oldConfig = Read-IniFile -Path $iniPath
}

New-Item -ItemType Directory -Force -Path (Join-Path $GameDir "scripts") | Out-Null

$loaderSource = Join-Path $releaseDir "dinput8.dll"
$loaderDest = Join-Path $GameDir "dinput8.dll"
try {
    Copy-Item -LiteralPath $loaderSource -Destination $loaderDest -Force
} catch {
    if (-not (Test-Path -LiteralPath $loaderDest)) {
        throw
    }
    Write-Warning "Skipped dinput8.dll because it is currently locked; existing loader remains installed."
}

$asiPath = Join-Path $GameDir "scripts\AC5Tools.asi"
try {
    Copy-Item -LiteralPath (Join-Path $releaseDir "scripts\AC5Tools.asi") -Destination $asiPath -Force
} catch {
    throw "Could not update scripts\AC5Tools.asi because it is locked. Close Assassin's Creed Rogue, then run install_to_game.ps1 again. The INI was not changed."
}

$native = @"
using System;
using System.Runtime.InteropServices;

public static class IniNative {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool WritePrivateProfileString(
        string section,
        string key,
        string value,
        string filePath);
}
"@

if (-not ("IniNative" -as [type])) {
    Add-Type $native
}

function Set-IniValue {
    param(
        [Parameter(Mandatory = $true)][string] $Path,
        [Parameter(Mandatory = $true)][string] $Section,
        [Parameter(Mandatory = $true)][string] $Key,
        [Parameter(Mandatory = $true)][string] $Value
    )

    $ok = [IniNative]::WritePrivateProfileString($Section, $Key, $Value, $Path)
    if (-not $ok) {
        throw "Failed to write [$Section] $Key to $Path"
    }
}

$defaultIniPath = Join-Path $releaseDir "scripts\AC5Tools.ini"
$tempIniPath = Join-Path (Join-Path $GameDir "scripts") ("AC5Tools.ini.tmp-{0}" -f [Guid]::NewGuid().ToString("N"))

try {
    Copy-Item -LiteralPath $defaultIniPath -Destination $tempIniPath -Force

    if ($null -ne $oldConfig) {
        $newDefaultConfig = Read-IniFile -Path $tempIniPath
        foreach ($section in $newDefaultConfig.Keys) {
            if (-not $oldConfig.ContainsKey($section)) {
                continue
            }
            foreach ($key in $newDefaultConfig[$section].Keys) {
                if ($oldConfig[$section].ContainsKey($key)) {
                    Set-IniValue -Path $tempIniPath -Section $section -Key $key -Value $oldConfig[$section][$key]
                }
            }
        }
    }

    if (Test-Path -LiteralPath $iniPath) {
        $backupPath = "{0}.bak-{1}" -f $iniPath, (Get-Date -Format "yyyyMMdd-HHmmss")
        Copy-Item -LiteralPath $iniPath -Destination $backupPath -Force
    }
    Move-Item -LiteralPath $tempIniPath -Destination $iniPath -Force
} catch {
    if (Test-Path -LiteralPath $tempIniPath) {
        Remove-Item -LiteralPath $tempIniPath -Force
    }
    throw
}

Get-Item -LiteralPath (Join-Path $GameDir "dinput8.dll"),
                      (Join-Path $GameDir "scripts\AC5Tools.asi"),
                      $iniPath |
    Select-Object FullName, Length, LastWriteTime
