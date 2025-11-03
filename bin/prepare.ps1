#!/usr/bin/env pwsh
#
#   prepare.ps1 - Prepare for building with an APP
#
#   This exports the app's config files from the apps config directory to the
#   TOP/state/config directory.
#
param(
    [Parameter(Mandatory=$false)]
    [string]$APP = "unit",

    [string]$FROM
)

$TOP = Get-Location
$ErrorActionPreference = "Stop"

$CONFIG = Join-Path $TOP "state\config"
$SITE = Join-Path $TOP "state\site"
$HDR = Join-Path $TOP "include\config.h"
$JSON = Join-Path $TOP "bin\json.exe"
$DB = Join-Path $TOP "state\db"

Write-Host "   [Prepare] Building with $APP app"

if (-not (Test-Path $JSON)) {
    Write-Host "   [Trace] Making json tool"
    & "$TOP\bin\prep-build.ps1"
}

# Create required state directories
New-Item -ItemType Directory -Force -Path "$TOP\state\certs" | Out-Null
New-Item -ItemType Directory -Force -Path "$TOP\state\site" | Out-Null
New-Item -ItemType Directory -Force -Path "$TOP\state\config" | Out-Null
New-Item -ItemType Directory -Force -Path "$TOP\state\db" | Out-Null

#
# Make certs if required and install
#
$rootsCrtExists = $false
if (Test-Path "$TOP\build") {
    Get-ChildItem -Path "$TOP\build" -Directory | ForEach-Object {
        if (Test-Path "$_\bin\roots.crt") {
            $rootsCrtExists = $true
        }
    }
}

if (-not $rootsCrtExists) {
    Write-Host "   [Trace] Installing certs"
    Get-ChildItem -Path "$TOP\build" -Directory | ForEach-Object {
        New-Item -ItemType Directory -Force -Path "$_\bin" | Out-Null
        Copy-Item -Path "$TOP\certs\roots.crt" -Destination "$_\bin\roots.crt" -Force
    }
    Copy-Item -Path "$TOP\certs\*.crt" -Destination "$TOP\state\certs" -Force
    Copy-Item -Path "$TOP\certs\*.key" -Destination "$TOP\state\certs" -Force
}

Push-Location "apps\$APP"

#
#   Detect app changes
#   Update the "app" field in the ioto.json5 file
#
$configFile = Join-Path $CONFIG "ioto.json5"
$prior = & $JSON -n --default "_" app $configFile 2>$null
if ($null -eq $prior) {
    $prior = "_"
}

if ($prior -ne $APP) {
    Write-Host "   [Trace] App changed from $prior to $APP, cleaning build"
    if (Test-Path "$TOP\build") {
        Get-ChildItem -Path "$TOP\build" -Directory | ForEach-Object {
            Remove-Item -Path "$_\bin\*" -Force -Recurse -ErrorAction SilentlyContinue
            Remove-Item -Path "$_\obj\*" -Force -Recurse -ErrorAction SilentlyContinue
        }
    }
    Remove-Item -Path $HDR -Force -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path $CONFIG | Out-Null

#
#   Export updated config files
#
$configFiles = @('db.json5', 'device.json5', 'display.json5', 'ioto.json5', 'local.json5', 'schema.json5', 'web.json5')

foreach ($file in $configFiles) {
    #
    #   Check if this file exists in the source directory and process if present/relevant
    #
    $sourceFile = Join-Path "config" $file
    $destFile = Join-Path $CONFIG $file

    if (Test-Path $sourceFile) {
        # Check if the file is valid JSON
        & $JSON -q --check $sourceFile
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Cannot parse $(Get-Location)\config\$file"
        }

        # Copy if destination doesn't exist, is empty, or source is newer
        $shouldCopy = $false
        if (-not (Test-Path $destFile)) {
            $shouldCopy = $true
        } elseif ((Get-Item $destFile).Length -eq 0) {
            $shouldCopy = $true
        } elseif ((Get-Item $sourceFile).LastWriteTime -gt (Get-Item $destFile).LastWriteTime) {
            $shouldCopy = $true
        }

        if ($shouldCopy) {
            Write-Host "      [Copy] json --blend config\$file >state\config\$file"
            cmd /c "$JSON --blend $sourceFile > $destFile"
            if ((Get-Item $destFile).Length -eq 0) {
                Write-Host "Error blending config\$file"
                exit 255
            }
        }
    }
}

#
#   Update the config.h file if required
#
$iotojson5 = "config\ioto.json5"
if ((-not (Test-Path $HDR)) -or ((Get-Item $iotojson5).LastWriteTime -gt (Get-Item $HDR).LastWriteTime)) {
    Write-Host "      [Create] $HDR"
    cmd /c "$JSON --header services $configFile > $HDR"
    $appHeader = & $JSON --header app $configFile
    $appHeader = $appHeader -replace '"', ''
    Add-Content -Path $HDR -Value $appHeader
}

#
#   Copy web site if it exists and if newer
#
if (Test-Path "site") {
    if (-not (Test-Path $SITE)) {
        Write-Host "      [Copy] Web site for $APP"
        New-Item -ItemType Directory -Force -Path $SITE | Out-Null
    }
    Get-ChildItem -Path $SITE -File -Recurse | Remove-Item -Force
    Copy-Item -Path "site\*" -Destination $SITE -Recurse -Force
}

#
#   Copy schema if it exists and if newer
#
if (Test-Path "ui\src\schema.json5") {
    $uiSchema = "ui\src\schema.json5"
    $configSchema = Join-Path $CONFIG "schema.json5"

    if ((Get-Item $uiSchema).LastWriteTime -gt (Get-Item $configSchema).LastWriteTime) {
        Write-Host "     [Copy] cp ui\src\schema.json5 $configSchema"
        Copy-Item -Path $uiSchema -Destination $configSchema -Force
        Remove-Item -Path "$DB\*.jnl" -Force -ErrorAction SilentlyContinue
        Remove-Item -Path "$DB\*.db" -Force -ErrorAction SilentlyContinue
    }
}

#
#   Copy signatures if it exists and if newer
#
$sigSource = "config\signatures.json5"
if (Test-Path $sigSource) {
    $sigDest = Join-Path $CONFIG "signatures.json5"
    if ((Get-Item $sigSource).LastWriteTime -gt (Get-Item $sigDest).LastWriteTime) {
        Write-Host "       [Make] make-sig config\signatures.json5 $sigDest"
        Remove-Item -Path $sigDest -Force -ErrorAction SilentlyContinue

        $schemaFile = Join-Path $CONFIG "schema.json5"
        $supportQuery = Join-Path $TOP "paks\dev-schemas\parts\SupportQuery.json5"
        $queryFile = Join-Path $TOP "paks\dev-schemas\parts\Query.json5"
        $matchFile = Join-Path $TOP "paks\dev-schemas\parts\Match.json5"

        & make-sig `
            --blend "Schema=$schemaFile" `
            --blend "SupportQuery=$supportQuery" `
            --blend "Query=$queryFile" `
            --blend "Match=$matchFile" `
            $sigSource $sigDest
    }
}
Pop-Location
