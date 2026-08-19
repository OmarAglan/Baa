[CmdletBinding()]
param(
    [string]$DestinationDirectory = "",
    [string]$CacheDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($DestinationDirectory)) {
    $DestinationDirectory = Join-Path $root "gcc"
}
if ([string]::IsNullOrWhiteSpace($CacheDirectory)) {
    $CacheDirectory = Join-Path $root ".cache\toolchains"
}

$release = "16.1.0posix-14.0.0-ucrt-r2"
$archiveName = "winlibs-x86_64-posix-seh-gcc-16.1.0-mingw-w64ucrt-14.0.0-r2.7z"
$archiveSha256 = "62FB8588D2DEEE7D662DBCBD386702ADBF19643764C971C38AA4839472EEE232"
$archiveUrl = "https://github.com/brechtsanders/winlibs_mingw/releases/download/$release/$archiveName"

function Find-7Zip {
    $command = Get-Command 7z -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    foreach ($candidate in @(
        (Join-Path $env:ProgramFiles "7-Zip\7z.exe"),
        (Join-Path ${env:ProgramFiles(x86)} "7-Zip\7z.exe")
    )) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    throw "7-Zip is required to prepare the private linker bundle."
}

function Assert-SafeWorkspacePath {
    param([string]$Path, [string]$Name)
    $fullPath = [IO.Path]::GetFullPath($Path).TrimEnd('\')
    $fullRoot = [IO.Path]::GetFullPath($root).TrimEnd('\') + "\"
    if (-not $fullPath.StartsWith($fullRoot, [StringComparison]::OrdinalIgnoreCase)) {
        throw "$Name must remain inside the Baa repository: $fullPath"
    }
    return $fullPath
}

$DestinationDirectory = Assert-SafeWorkspacePath $DestinationDirectory "DestinationDirectory"
$CacheDirectory = Assert-SafeWorkspacePath $CacheDirectory "CacheDirectory"
[IO.Directory]::CreateDirectory($CacheDirectory) | Out-Null
$archive = Join-Path $CacheDirectory $archiveName

if (Test-Path -LiteralPath $archive -PathType Leaf) {
    $cachedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    if ($cachedHash -ne $archiveSha256) {
        throw "Cached WinLibs archive hash mismatch: $archive"
    }
}
else {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $archiveUrl -OutFile $archive -UseBasicParsing
    $downloadedHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $archive).Hash
    if ($downloadedHash -ne $archiveSha256) {
        Remove-Item -LiteralPath $archive -Force
        throw "Downloaded WinLibs archive hash mismatch."
    }
}

$extractDirectory = Assert-SafeWorkspacePath (Join-Path $root ".baa_toolchain_extract_$PID") "ExtractDirectory"
$sevenZip = Find-7Zip
try {
    if (Test-Path -LiteralPath $extractDirectory) {
        Remove-Item -LiteralPath $extractDirectory -Recurse -Force
    }
    & $sevenZip x $archive "-o$extractDirectory" -y | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "WinLibs archive extraction failed." }

    $extracted = Join-Path $extractDirectory "mingw64"
    if (-not (Test-Path -LiteralPath $extracted -PathType Container)) {
        $extracted = $extractDirectory
    }

    if (Test-Path -LiteralPath $DestinationDirectory) {
        Remove-Item -LiteralPath $DestinationDirectory -Recurse -Force
    }
    [IO.Directory]::CreateDirectory($DestinationDirectory) | Out-Null

    $linkerFiles = @(
        "bin\gcc.exe",
        "bin\ld.exe",
        "bin\libiconv-2.dll",
        "bin\libintl-8.dll",
        "bin\libwinpthread-1.dll",
        "bin\libz.dll",
        "bin\libzstd.dll",
        "lib\gcc\x86_64-w64-mingw32\16.1.0\libgcc.a",
        "lib\gcc\x86_64-w64-mingw32\16.1.0\libgcc_eh.a",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\cc1.exe",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\collect2.exe",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libgmp-10.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libiconv-2.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libisl-23.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\liblto_plugin.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libmpc-3.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libmpfr-6.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libwinpthread-1.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libz.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\libzstd.dll",
        "libexec\gcc\x86_64-w64-mingw32\16.1.0\lto-wrapper.exe",
        "x86_64-w64-mingw32\bin\as.exe",
        "x86_64-w64-mingw32\bin\ld.exe",
        "x86_64-w64-mingw32\bin\libiconv-2.dll",
        "x86_64-w64-mingw32\bin\libintl-8.dll",
        "x86_64-w64-mingw32\bin\libwinpthread-1.dll",
        "x86_64-w64-mingw32\bin\libz.dll",
        "x86_64-w64-mingw32\bin\libzstd.dll"
    )
    foreach ($relativePath in $linkerFiles) {
        $source = Join-Path $extracted $relativePath
        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            throw "WinLibs archive is missing required linker file: $relativePath"
        }
        $destination = Join-Path $DestinationDirectory $relativePath
        [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
        Copy-Item -LiteralPath $source -Destination $destination
    }

    $targetLibraries = Join-Path $extracted "x86_64-w64-mingw32\lib"
    if (-not (Test-Path -LiteralPath $targetLibraries -PathType Container)) {
        throw "WinLibs archive is missing the target import-library directory."
    }
    Copy-Item -LiteralPath $targetLibraries -Destination (Join-Path $DestinationDirectory "x86_64-w64-mingw32\lib") -Recurse

    $gcc = Join-Path $DestinationDirectory "bin\gcc.exe"
    $ld = Join-Path $DestinationDirectory "bin\ld.exe"
    foreach ($required in @($gcc, $ld)) {
        if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
            throw "Prepared linker bundle is missing: $required"
        }
    }

    $probeDirectory = Join-Path $extractDirectory "probe-ascii"
    [IO.Directory]::CreateDirectory($probeDirectory) | Out-Null
    $probeAssembly = Join-Path $probeDirectory "probe.s"
    $probeObject = Join-Path $probeDirectory "probe.o"
    $probeProgram = Join-Path $probeDirectory "probe.exe"
    $assembly = @(
        ".text",
        ".globl mainCRTStartup",
        ".extern ExitProcess",
        "mainCRTStartup:",
        '  sub $40, %rsp',
        "  xor %ecx, %ecx",
        "  call ExitProcess"
    ) -join "`n"
    [IO.File]::WriteAllText($probeAssembly, $assembly + "`n", [Text.Encoding]::ASCII)
    $assembler = Join-Path $extracted "x86_64-w64-mingw32\bin\as.exe"
    & $assembler $probeAssembly -o $probeObject
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $probeObject -PathType Leaf)) {
        throw "WinLibs assembler could not create the linker probe object."
    }
    $unicodeName = (-join [char[]](0x0645, 0x0633, 0x0627, 0x0631)) + " path"
    $unicodeDirectory = Join-Path $extractDirectory $unicodeName
    [IO.Directory]::CreateDirectory($unicodeDirectory) | Out-Null
    $unicodeObject = Join-Path $unicodeDirectory "probe.o"
    $unicodeProgram = Join-Path $unicodeDirectory "probe.exe"
    Copy-Item -LiteralPath $probeObject -Destination $unicodeObject
    $unicodeProbePassed = $false

    $oldPath = $env:PATH
    try {
        $env:PATH = "$(Join-Path $DestinationDirectory 'bin');$env:SystemRoot\System32;$env:SystemRoot"
        & $gcc --version | Select-Object -First 1
        if ($LASTEXITCODE -ne 0) { throw "Private GCC version probe failed." }
        & $ld --version | Select-Object -First 1
        if ($LASTEXITCODE -ne 0) { throw "Private linker version probe failed." }
        & $gcc -nostartfiles "-Wl,-e,mainCRTStartup" $probeObject -lkernel32 -o $probeProgram
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $probeProgram -PathType Leaf)) {
            throw "Private linker failed its dependency-closure probe."
        }
        & $probeProgram
        if ($LASTEXITCODE -ne 0) { throw "Private linker probe executable failed." }
        $savedPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        & $gcc -nostartfiles "-Wl,-e,mainCRTStartup" $unicodeObject -lkernel32 -o $unicodeProgram 2>$null
        $unicodeExitCode = $LASTEXITCODE
        $ErrorActionPreference = $savedPreference
        $unicodeProbePassed =
            ($unicodeExitCode -eq 0) -and
            (Test-Path -LiteralPath $unicodeProgram -PathType Leaf)
        if ($unicodeProbePassed) {
            & $unicodeProgram
            $unicodeProbePassed = $LASTEXITCODE -eq 0
        }
    }
    finally {
        $env:PATH = $oldPath
    }

    $gccHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $gcc).Hash
    $manifest = @(
        "format=baa-portable-toolchain-v1",
        "target=x86_64-w64-mingw32",
        "gcc_version=16.1.0",
        "gcc_sha256=$gccHash",
        "archive_sha256=$archiveSha256",
        "source_url=$archiveUrl",
        "unicode_paths=direct",
        "pei386_runtime_relocator=retain",
        "purpose=Baa hosted linking until the native Nazm linker is admitted"
    ) -join "`n"
    [IO.File]::WriteAllText(
        (Join-Path $DestinationDirectory "BAA-TOOLCHAIN-MANIFEST.txt"),
        $manifest + "`n",
        (New-Object Text.UTF8Encoding($false)))
}
finally {
    if (Test-Path -LiteralPath $extractDirectory) {
        Remove-Item -LiteralPath $extractDirectory -Recurse -Force
    }
}

$bundle = Get-ChildItem -LiteralPath $DestinationDirectory -File -Recurse | Measure-Object Length -Sum
Write-Output ("Prepared {0} files ({1:N1} MB) in {2}." -f $bundle.Count, ($bundle.Sum / 1MB), $DestinationDirectory)
if ($unicodeProbePassed) {
    Write-Output "Direct Unicode paths passed on this Windows code page."
}
else {
    Write-Output "This Windows code page requires Baa's no-copy short-path adapter for GCC/LD."
}

# The direct-Unicode probe is advisory: a failure selects Baa's supported
# short-path adapter. Do not leak that expected native-process status to callers
# such as the GitHub Actions PowerShell wrapper, which exits with LASTEXITCODE.
exit 0
