#!/usr/bin/env pwsh
#
#   prep-build.ps1 - Make the required build tools
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

# Always try to initialize 64-bit Visual Studio environment to ensure we get the x64 compiler
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vsWhere) {
    Write-Host "Detecting Visual Studio installation..."
    $vsPath = & $vsWhere -latest -property installationPath
    if ($vsPath) {
        Write-Host "Found Visual Studio at: $vsPath"
        $vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
        if (Test-Path $vcvarsPath) {
            Write-Host "Initializing 64-bit Visual Studio environment from: $vcvarsPath"
            # Run vcvars64 with x64 architecture explicitly and capture environment changes
            $tempFile = [System.IO.Path]::GetTempFileName()
            cmd /c "`"$vcvarsPath`" x64 > nul && set" | Out-File -FilePath $tempFile -Encoding ascii
            Get-Content $tempFile | ForEach-Object {
                if ($_ -match '^([^=]+)=(.*)$') {
                    [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
                }
            }
            Remove-Item $tempFile
        }
    }
}

# Now check for MSVC compiler (should be 64-bit after vcvars64)
$clFound = Get-Command cl -ErrorAction SilentlyContinue
if ($clFound) {
    Write-Host "Compiler found at: $($clFound.Source)"
}

if ($clFound) {
    $compiler = "cl"
    $compilerArgs = @(
        "/DJSON_SOLO=1",
        "/Fo:bin\",
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
