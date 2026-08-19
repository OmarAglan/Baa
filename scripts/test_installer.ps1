param(
    [string]$Installer = "",
    [string]$InstallDirectory = "",
    [string]$NazmDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Installer)) {
    $Installer = Join-Path $root "dist\installer\baa-setup-0.6.0-x64.exe"
}
if ([string]::IsNullOrWhiteSpace($InstallDirectory)) {
    $InstallDirectory = Join-Path $env:LOCALAPPDATA "Temp\BaaInstallerContract"
}
if ([string]::IsNullOrWhiteSpace($NazmDirectory)) {
    $NazmDirectory = Join-Path $root "..\Nazm\build\windows-release"
}
$Installer = (Resolve-Path -LiteralPath $Installer).Path
$NazmDirectory = (Resolve-Path -LiteralPath $NazmDirectory).Path
$checksumPath = $Installer + ".sha256"
if (-not (Test-Path -LiteralPath $checksumPath -PathType Leaf)) {
    throw "Installer checksum file was not found: $checksumPath"
}
$checksumLine = [IO.File]::ReadAllText($checksumPath, [Text.Encoding]::ASCII).Trim()
if ($checksumLine -notmatch '^([0-9A-Fa-f]{64}) \*(.+)$') {
    throw "Installer checksum file has an invalid format."
}
if ($Matches[2] -cne [IO.Path]::GetFileName($Installer)) {
    throw "Installer checksum names the wrong file."
}
$actualInstallerHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $Installer).Hash
if ($Matches[1] -ine $actualInstallerHash) {
    throw "Installer SHA-256 verification failed."
}
$arabicNazm = (-join [char[]](0x0646, 0x0638, 0x0645)) + ".exe"
if (-not (Test-Path -LiteralPath (Join-Path $NazmDirectory $arabicNazm) -PathType Leaf)) {
    throw "Nazm Arabic command was not found in $NazmDirectory."
}
if (Test-Path -LiteralPath $InstallDirectory) {
    throw "Installer test directory already exists: $InstallDirectory"
}

$baaExecutable = Join-Path $InstallDirectory "baa.exe"
$uninstaller = Join-Path $InstallDirectory "unins000.exe"
$markerKey = "HKCU:\Software\BaaEcosystem\Baa"
$programDirectory = Join-Path $env:LOCALAPPDATA "Temp\BaaInstallerProgram"
$programOutput = Join-Path $programDirectory "hello.exe"
$gasProgramOutput = Join-Path $programDirectory "hello-gas.exe"
$source = Join-Path $root "examples\hello_world.baa"
$installed = $false

function Wait-InstallerState {
    param([string]$Description, [scriptblock]$Condition)
    for ($attempt = 0; $attempt -lt 300; $attempt++) {
        if (& $Condition) { return }
        Start-Sleep -Milliseconds 200
    }
    throw "Timed out waiting for $Description."
}

try {
    $setupProcess = Start-Process -FilePath $Installer -ArgumentList @(
        "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/SP-",
        "/CURRENTUSER", "/DIR=$InstallDirectory"
    ) -WindowStyle Hidden -Wait -PassThru
    if ($setupProcess.ExitCode -ne 0) {
        throw "Baa installer failed with exit code $($setupProcess.ExitCode)."
    }
    $installed = $true
    Wait-InstallerState "Baa installation" {
        (Test-Path -LiteralPath $baaExecutable -PathType Leaf) -and
        (Test-Path -LiteralPath $uninstaller -PathType Leaf) -and
        (Test-Path -LiteralPath $markerKey)
    }

    foreach ($required in @(
        "libbaa_runtime.a",
        "stdlib\baalib.baahd",
        "gcc\bin\gcc.exe",
        "gcc\bin\ld.exe",
        "gcc\BAA-TOOLCHAIN-MANIFEST.txt"
    )) {
        if (-not (Test-Path -LiteralPath (Join-Path $InstallDirectory $required) -PathType Leaf)) {
            throw "Baa installer is missing $required."
        }
    }

    $pathValue = (Get-ItemProperty -Path "HKCU:\Environment" -Name Path).Path
    $baaPathMatches = @($pathValue -split ";" | Where-Object {
        $_.Trim().Trim('"').TrimEnd('\') -ieq $InstallDirectory.TrimEnd('\')
    })
    $gccPathMatches = @($pathValue -split ";" | Where-Object {
        $_.Trim().Trim('"').TrimEnd('\') -ieq
            (Join-Path $InstallDirectory "gcc\bin").TrimEnd('\')
    })
    if ($baaPathMatches.Count -ne 1) { throw "Baa PATH entry was not added exactly once." }
    if ($gccPathMatches.Count -ne 0) { throw "Private Baa GCC leaked into PATH." }
    if ((Get-ItemPropertyValue -Path $markerKey -Name PathOwned) -ne 1) {
        throw "Baa installer did not record PATH ownership."
    }
    if ((Get-ItemPropertyValue -Path "HKCU:\Environment" -Name BAA_HOME) -ine
        $InstallDirectory) { throw "BAA_HOME does not point to the installation." }
    if ((Get-ItemPropertyValue -Path "HKCU:\Environment" -Name BAA_STDLIB) -ine
        (Join-Path $InstallDirectory "stdlib")) {
        throw "BAA_STDLIB does not point to the installed standard library."
    }

    [IO.Directory]::CreateDirectory($programDirectory) | Out-Null
    $oldPath = $env:PATH
    $oldHome = $env:BAA_HOME
    $oldStdlib = $env:BAA_STDLIB
    $hadNazmOverride = Test-Path Env:BAA_NAZM
    $oldNazmOverride = $env:BAA_NAZM
    try {
        $env:PATH = "$NazmDirectory;$env:SystemRoot\System32;$env:SystemRoot"
        $env:BAA_HOME = $InstallDirectory
        $env:BAA_STDLIB = Join-Path $InstallDirectory "stdlib"
        Remove-Item Env:BAA_NAZM -ErrorAction SilentlyContinue

        & $baaExecutable --version
        if ($LASTEXITCODE -ne 0) { throw "Installed Baa version probe failed." }
        & $baaExecutable $source -o $programOutput
        if ($LASTEXITCODE -ne 0 -or
            -not (Test-Path -LiteralPath $programOutput -PathType Leaf)) {
            throw "Installed Baa failed to compile and link through PATH Nazm."
        }
        & $programOutput
        if ($LASTEXITCODE -ne 0) { throw "Program linked by installed Baa failed to run." }
        & $baaExecutable --assembler=gas $source -o $gasProgramOutput
        if ($LASTEXITCODE -ne 0 -or
            -not (Test-Path -LiteralPath $gasProgramOutput -PathType Leaf)) {
            throw "Installed Baa explicit GAS rollback failed."
        }
        & $gasProgramOutput
        if ($LASTEXITCODE -ne 0) { throw "Program built by the installed GAS rollback failed to run." }
    }
    finally {
        $env:PATH = $oldPath
        $env:BAA_HOME = $oldHome
        $env:BAA_STDLIB = $oldStdlib
        if ($hadNazmOverride) { $env:BAA_NAZM = $oldNazmOverride }
        else { Remove-Item Env:BAA_NAZM -ErrorAction SilentlyContinue }
    }
}
finally {
    if ($installed -and (Test-Path -LiteralPath $uninstaller -PathType Leaf)) {
        $uninstallProcess = Start-Process -FilePath $uninstaller -ArgumentList @(
            "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART"
        ) -WindowStyle Hidden -Wait -PassThru
        if ($uninstallProcess.ExitCode -ne 0) {
            throw "Baa uninstaller failed with exit code $($uninstallProcess.ExitCode)."
        }
        Wait-InstallerState "Baa uninstall cleanup" {
            -not (Test-Path -LiteralPath $InstallDirectory) -and
            -not (Test-Path -LiteralPath $markerKey)
        }
    }
    if (Test-Path -LiteralPath $programDirectory) {
        $resolvedProgram = [IO.Path]::GetFullPath($programDirectory)
        $resolvedTemp = [IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "Temp"))
        if (-not $resolvedProgram.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove test output outside the user Temp directory."
        }
        Remove-Item -LiteralPath $resolvedProgram -Recurse -Force
    }
}

$remainingPath = (Get-ItemProperty -Path "HKCU:\Environment" -Name Path).Path
$remainingMatches = @($remainingPath -split ";" | Where-Object {
    $_.Trim().Trim('"').TrimEnd('\') -ieq $InstallDirectory.TrimEnd('\')
})
if ($remainingMatches.Count -ne 0) { throw "Baa uninstaller left its PATH entry." }
if (Test-Path -LiteralPath $InstallDirectory) { throw "Baa uninstaller left installation files." }
if (Test-Path -LiteralPath $markerKey) { throw "Baa uninstaller left its ownership marker." }
$remainingEnvironment = Get-ItemProperty -Path "HKCU:\Environment"
$remainingHome = $remainingEnvironment.BAA_HOME
$remainingStdlib = $remainingEnvironment.BAA_STDLIB
if ($remainingHome -ieq $InstallDirectory) { throw "Baa uninstaller left its owned BAA_HOME value." }
if ($remainingStdlib -ieq (Join-Path $InstallDirectory "stdlib")) {
    throw "Baa uninstaller left its owned BAA_STDLIB value."
}

Write-Output "Baa installer contract passed."
