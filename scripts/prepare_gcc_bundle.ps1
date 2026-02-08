# prepare_gcc_bundle.ps1
# -------------------------------------------------------
# Downloads and extracts a minimal MinGW-w64 GCC toolchain
# into a gcc/ folder at the project root, ready for the
# Inno Setup installer (setup.iss) to bundle.
#
# Usage (from the project root):
#   powershell -ExecutionPolicy Bypass -File scripts\prepare_gcc_bundle.ps1
#
# Requirements: PowerShell 5.1+, internet access.
# -------------------------------------------------------

$ErrorActionPreference = "Stop"

# --- Configuration -----------------------------------------------------------
# WinLibs standalone build of GCC + MinGW-w64 (ucrt, no LLVM, no Boost).
# This is a self-contained archive — no installer required.
$GCC_URL = "https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-19.1.1-12.0.0-ucrt-r2/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r2.7z"
$ARCHIVE = "mingw64.7z"
$GCC_DIR = "gcc"            # Output folder (relative to project root)

# --- Helpers -----------------------------------------------------------------
function Ensure-7Zip {
    # Prefer 7z on PATH, then common install locations
    if (Get-Command 7z -ErrorAction SilentlyContinue) { return "7z" }
    $candidates = @(
        "$env:ProgramFiles\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    Write-Error "7-Zip is required to extract the archive. Install it from https://www.7-zip.org/"
    exit 1
}

# --- Main --------------------------------------------------------------------
Write-Host "=== Baa GCC Bundle Preparation ===" -ForegroundColor Cyan

# 1. Download if not already cached
if (-not (Test-Path $ARCHIVE)) {
    Write-Host "Downloading MinGW-w64 GCC..."
    Write-Host "  URL: $GCC_URL"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $GCC_URL -OutFile $ARCHIVE -UseBasicParsing
    Write-Host "  Downloaded: $ARCHIVE ($('{0:N1} MB' -f ((Get-Item $ARCHIVE).Length / 1MB)))"
}
else {
    Write-Host "Using cached archive: $ARCHIVE"
}

# 2. Extract to a temp folder
$TEMP_DIR = "mingw64_tmp"
if (Test-Path $TEMP_DIR) { Remove-Item $TEMP_DIR -Recurse -Force }

$sevenZip = Ensure-7Zip
Write-Host "Extracting with 7-Zip..."
& $sevenZip x $ARCHIVE -o"$TEMP_DIR" -y | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Error "Extraction failed"; exit 1 }

# The archive extracts to mingw64_tmp\mingw64\...
$EXTRACTED = Join-Path $TEMP_DIR "mingw64"
if (-not (Test-Path $EXTRACTED)) {
    # Some archives don't have the extra wrapper directory
    $EXTRACTED = $TEMP_DIR
}

# 3. Copy only the necessary subdirectories to gcc/
if (Test-Path $GCC_DIR) { Remove-Item $GCC_DIR -Recurse -Force }
New-Item -ItemType Directory -Path $GCC_DIR | Out-Null

$needed = @("bin", "lib", "libexec", "x86_64-w64-mingw32")
foreach ($sub in $needed) {
    $src = Join-Path $EXTRACTED $sub
    if (Test-Path $src) {
        Write-Host "  Copying $sub..."
        Copy-Item -Path $src -Destination (Join-Path $GCC_DIR $sub) -Recurse
    }
}

# 4. Verify that gcc.exe exists
$gccExe = Join-Path $GCC_DIR "bin\gcc.exe"
if (Test-Path $gccExe) {
    $ver = & $gccExe --version | Select-Object -First 1
    Write-Host "`nSuccess! Bundled GCC: $ver" -ForegroundColor Green
}
else {
    Write-Error "gcc.exe not found in $GCC_DIR\bin — something went wrong."
    exit 1
}

# 5. Cleanup temp files
Remove-Item $TEMP_DIR -Recurse -Force
Write-Host "Temp files cleaned up."

# 6. Print size
$size = (Get-ChildItem -Recurse $GCC_DIR | Measure-Object -Property Length -Sum).Sum
Write-Host "Bundle size: $('{0:N1} MB' -f ($size / 1MB))"
Write-Host "`nThe $GCC_DIR/ folder is ready for the installer." -ForegroundColor Cyan
Write-Host "Run Inno Setup on setup.iss to build the installer."
