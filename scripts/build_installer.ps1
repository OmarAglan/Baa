param(
    [string]$Version = "0.6.0",
    [string]$BuildDirectory = "",
    [string]$ToolchainDirectory = "",
    [string]$NazmExecutable = "",
    [string]$IsccPath = "",
    [switch]$SkipBuild,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $root "build"
}
if ([string]::IsNullOrWhiteSpace($ToolchainDirectory)) {
    $ToolchainDirectory = Join-Path $root "gcc"
}
if ([string]::IsNullOrWhiteSpace($NazmExecutable)) {
    $siblingNazm = Join-Path $root "..\Nazm\build\windows-release\nazm.exe"
    if (Test-Path -LiteralPath $siblingNazm -PathType Leaf) {
        $NazmExecutable = $siblingNazm
    }
}

& python (Join-Path $root "scripts\check_version_sync.py")
if ($LASTEXITCODE -ne 0) { throw "Baa version synchronization check failed." }
$cmakeText = Get-Content -Raw -Encoding UTF8 -LiteralPath (Join-Path $root "CMakeLists.txt")
if ($cmakeText -notmatch "project\(baa\s+VERSION\s+$([regex]::Escape($Version))") {
    throw "Baa version $Version does not match CMakeLists.txt."
}

if (-not $SkipBuild) {
    & cmake -S $root -B $BuildDirectory -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
    if ($LASTEXITCODE -ne 0) { throw "Baa CMake configure failed." }
    & cmake --build $BuildDirectory --clean-first --target baa baa_runtime
    if ($LASTEXITCODE -ne 0) { throw "Baa build failed." }
}

$baaExecutable = Join-Path $BuildDirectory "baa.exe"
$runtimeLibrary = Join-Path $BuildDirectory "libbaa_runtime.a"
foreach ($file in @($baaExecutable, $runtimeLibrary)) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required Baa installer input is missing: $file"
    }
}

if (-not $SkipTests) {
    if ([string]::IsNullOrWhiteSpace($NazmExecutable) -or
        -not (Test-Path -LiteralPath $NazmExecutable -PathType Leaf)) {
        throw "Nazm is required for Baa quick QA. Pass -NazmExecutable."
    }
    $oldBaa = $env:BAA
    $oldNazm = $env:BAA_NAZM
    try {
        $env:BAA = (Resolve-Path -LiteralPath $baaExecutable).Path
        $env:BAA_NAZM = (Resolve-Path -LiteralPath $NazmExecutable).Path
        & python (Join-Path $root "scripts\qa_run.py") --mode quick
        if ($LASTEXITCODE -ne 0) { throw "Baa quick QA failed." }
    }
    finally {
        $env:BAA = $oldBaa
        $env:BAA_NAZM = $oldNazm
    }
}

$requiredToolchainFiles = @(
    "bin\gcc.exe",
    "bin\ld.exe",
    "BAA-TOOLCHAIN-MANIFEST.txt"
)
foreach ($relative in $requiredToolchainFiles) {
    $path = Join-Path $ToolchainDirectory $relative
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "Required private linker input is missing: $path"
    }
}

$manifestPath = Join-Path $ToolchainDirectory "BAA-TOOLCHAIN-MANIFEST.txt"
$manifestLines = [IO.File]::ReadAllLines((Resolve-Path -LiteralPath $manifestPath).Path)
foreach ($requiredLine in @(
    "format=baa-portable-toolchain-v1",
    "target=x86_64-w64-mingw32",
    "unicode_paths=direct",
    "pei386_runtime_relocator=retain"
)) {
    if ($manifestLines -notcontains $requiredLine) {
        throw "Private linker manifest is missing '$requiredLine'."
    }
}
$recordedHashLine = $manifestLines | Where-Object { $_ -like "gcc_sha256=*" } | Select-Object -First 1
$gccPath = Join-Path $ToolchainDirectory "bin\gcc.exe"
$actualGccHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $gccPath).Hash
if (-not $recordedHashLine -or $recordedHashLine.Substring(11) -ine $actualGccHash) {
    throw "Private linker manifest GCC hash does not match gcc.exe."
}

if ([string]::IsNullOrWhiteSpace($IsccPath)) {
    $isccCommand = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($isccCommand) { $IsccPath = $isccCommand.Source }
}
if ([string]::IsNullOrWhiteSpace($IsccPath)) {
    $IsccPath = @(
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe",
        "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
        "C:\Program Files\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($IsccPath) -or
    -not (Test-Path -LiteralPath $IsccPath -PathType Leaf)) {
    throw "Inno Setup 6 compiler was not found. Pass -IsccPath explicitly."
}

$resolvedBuild = (Resolve-Path -LiteralPath $BuildDirectory).Path
$resolvedToolchain = (Resolve-Path -LiteralPath $ToolchainDirectory).Path
Push-Location $root
try {
    & $IsccPath "/DMyAppVersion=$Version" "/DBaaBinaryDir=$resolvedBuild" `
        "/DBaaToolchainDir=$resolvedToolchain" (Join-Path $root "setup.iss")
    if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE." }
}
finally {
    Pop-Location
}

$installer = Join-Path $root "dist\installer\baa-setup-$Version-x64.exe"
if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
    throw "Baa installer was not produced at $installer"
}
$installerHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $installer).Hash
$checksum = $installer + ".sha256"
[IO.File]::WriteAllText(
    $checksum,
    "$installerHash *$([IO.Path]::GetFileName($installer))`n",
    [Text.Encoding]::ASCII)
Write-Output $installer
Write-Output $checksum
