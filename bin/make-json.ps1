#!/usr/bin/env pwsh
#
#   make-json.ps1 - Make the required build tools
#
#   Usage: make-json.ps1
#
#   This builds the awesome "json" converter tool that can edit, query and convert JSON and JSON5 files.
#

$ErrorActionPreference = "Stop"

if (-not (Test-Path "bin")) {
    Write-Host "Must run from the top directory"
    exit 1
}

# Create required directories
New-Item -ItemType Directory -Force -Path "state\certs" | Out-Null
New-Item -ItemType Directory -Force -Path "state\config" | Out-Null
New-Item -ItemType Directory -Force -Path "state\db" | Out-Null
New-Item -ItemType Directory -Force -Path "state\site" | Out-Null

# Build json tool using cl (MSVC) or gcc (MinGW/Cygwin)
$compiler = $null
$compilerArgs = @()

# Try to find MSVC compiler
$clFound = Get-Command cl -ErrorAction SilentlyContinue
if (-not $clFound) {
    # Try to locate and initialize Visual Studio environment
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        Write-Host "Detecting Visual Studio installation..."
        $vsPath = & $vsWhere -latest -property installationPath
        if ($vsPath) {
            $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvarsPath) {
                Write-Host "Initializing Visual Studio environment..."
                # Run vcvars and capture environment changes
                $tempFile = [System.IO.Path]::GetTempFileName()
                cmd /c "`"$vcvarsPath`" > nul && set" | Out-File -FilePath $tempFile -Encoding ascii
                Get-Content $tempFile | ForEach-Object {
                    if ($_ -match '^([^=]+)=(.*)$') {
                        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
                    }
                }
                Remove-Item $tempFile
                $clFound = Get-Command cl -ErrorAction SilentlyContinue
            }
        }
    }
}

if ($clFound) {
    $compiler = "cl"
    $compilerArgs = @(
        "/DJSON_SOLO=1",
        "/Fe:bin\json.exe",
        "bin\json.c",
        "advapi32.lib",
        "user32.lib",
        "ws2_32.lib"
    )
}
# Try to find gcc (MinGW/Cygwin/WSL)
elseif (Get-Command gcc -ErrorAction SilentlyContinue) {
    $compiler = "gcc"
    $compilerArgs = @(
        "/DJSON_SOLO=1",
        "-o", "bin\json.exe",
        "bin\json.c",
        "-lm",
        "advapi32.lib",
        "user32.lib",
        "ws2_32.lib"
    )
}
# Try cc as fallback
elseif (Get-Command cc -ErrorAction SilentlyContinue) {
    $compiler = "cc"
    $compilerArgs = @(
        "/DJSON_SOLO=1",
        "-o", "bin\json.exe",
        "bin\json.c",
        "-lm",
        "advapi32.lib",
        "user32.lib",
        "ws2_32.lib"
    )
}
else {
    Write-Host "Error: No C compiler found (cl, gcc, or cc required)"
    exit 1
}

Write-Host "Building json tool with $compiler..."
Write-Host "Compiler arguments:"
foreach ($arg in $compilerArgs) {
    Write-Host "  $arg"
}
& $compiler @compilerArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Failed to build json tool"
    exit $LASTEXITCODE
}
