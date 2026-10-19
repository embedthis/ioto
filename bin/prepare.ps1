#!/usr/bin/env pwsh
#
#   prepare.ps1 - Prepare the app state directory
#
#   Called from the project top directory with APP name as argument.
#   Copies config files to the app's state directory.
#

param(
    [string]$APP = "http"
)

$ErrorActionPreference = "Stop"
$TOP = (Get-Location).Path
$JSON = Join-Path $TOP "build\bin\json.exe"

if (-not (Test-Path "src")) {
    Write-Host "Must run from the top directory"
    exit 1
}

if (-not (Test-Path "apps\$APP")) {
    Write-Host "      [Error] Unknown app `"$APP`""
    exit 255
}

#
#   Change to the app directory
#
Push-Location "apps\$APP"

New-Item -ItemType Directory -Force -Path "state" | Out-Null
& (Join-Path $TOP "bin\make-private.ps1") -Path "state"
New-Item -ItemType Directory -Force -Path "state\config" | Out-Null
New-Item -ItemType Directory -Force -Path "state\db" | Out-Null

#
#   Copy applicable config files to state/config
#
foreach ($file in @('db.json5', 'device.json5', 'display.json5', 'ioto.json5', 'local.json5', 'web.json5')) {
    $src = $file
    $dest = "state\config\$file"
    if (Test-Path $src) {
        $shouldCopy = $false
        if (-not (Test-Path $dest)) {
            $shouldCopy = $true
        } elseif ((Get-Item $src).LastWriteTime -gt (Get-Item $dest).LastWriteTime) {
            $shouldCopy = $true
        }
        if ($shouldCopy) {
            Write-Host "      [Copy] $file -> state\config\$file"
            Copy-Item -Path $src -Destination $dest -Force
        }
    }
}

#
#   Blend schema.json5 to state/config/schema.json5
#
if (Test-Path "schema.json5") {
    $dest = "state\config\schema.json5"
    $shouldBlend = $false
    if (-not (Test-Path $dest)) {
        $shouldBlend = $true
    } elseif ((Get-Item "schema.json5").LastWriteTime -gt (Get-Item $dest).LastWriteTime) {
        $shouldBlend = $true
    }
    if ($shouldBlend) {
        Write-Host "     [Blend] schema.json5 -> state\config\schema.json5"
        & $JSON --blend --out $dest schema.json5
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path $dest) -or (Get-Item $dest).Length -eq 0) {
            Write-Host "Error blending schema.json5"
            Pop-Location
            exit 255
        }
    }
}

#
#   Copy web site if it exists
#
if (Test-Path "site" -PathType Container) {
    if (-not (Test-Path "state\site" -PathType Container)) {
        Write-Host "      [Copy] Web site for $APP"
        New-Item -ItemType Directory -Force -Path "state\site" | Out-Null
    }
    Get-ChildItem -Path "state\site" -File -Recurse -ErrorAction SilentlyContinue | Remove-Item -Force
    Copy-Item -Path "site\*" -Destination "state\site" -Recurse -Force
}

#
#   Process signatures.json5 if it exists
#
if (Test-Path "signatures.json5") {
    $dest = "state\config\signatures.json5"
    $shouldProcess = $false
    if (-not (Test-Path $dest)) {
        $shouldProcess = $true
    } elseif ((Get-Item "signatures.json5").LastWriteTime -gt (Get-Item $dest).LastWriteTime) {
        $shouldProcess = $true
    }
    if ($shouldProcess) {
        Write-Host "      [Make] make-sig signatures.json5 state\config\signatures.json5"
        $schemaFile = "state\config\schema.json5"
        $supportQuery = Join-Path $TOP "schemas\parts\SupportQuery.json5"
        $queryFile = Join-Path $TOP "schemas\parts\Query.json5"
        $matchFile = Join-Path $TOP "schemas\parts\Match.json5"
        #
        #   make-sig is a Node script with a Unix shebang, which Windows cannot execute by name.
        #   Run it through bun, which the test tooling already requires, or node if that is what
        #   the machine has.
        #
        $makeSig = Join-Path $TOP "bin\make-sig"
        if (Get-Command bun -ErrorAction SilentlyContinue) {
            $runner = "bun"
        } elseif (Get-Command node -ErrorAction SilentlyContinue) {
            $runner = "node"
        } else {
            Write-Host "Error: bun or node is required to run bin\make-sig"
            Pop-Location
            exit 255
        }
        & $runner $makeSig `
            --blend "Schema=$schemaFile" `
            --blend "SupportQuery=$supportQuery" `
            --blend "Query=$queryFile" `
            --blend "Match=$matchFile" `
            signatures.json5 $dest
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: make-sig failed"
            Pop-Location
            exit 255
        }
    }
}

Pop-Location
