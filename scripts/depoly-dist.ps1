[CmdletBinding(DefaultParameterSetName = 'ByBuildDir')]
param(
    [Parameter(Mandatory = $true)]
    [string]$QtPath,

    [Parameter(ParameterSetName = 'ByBuildDir')]
    [string]$BuildDir = 'cmake-build-release-visual-studio',

    [Parameter(Mandatory = $true, ParameterSetName = 'ByExecutablePath')]
    [string]$ExecutablePath,

    [ValidateSet('Release', 'Debug', 'RelWithDebInfo', 'MinSizeRel')]
    [string]$Config = 'Release',

    [string]$AppName,

    [string]$DistRoot = 'dist',

    [string]$QmlDir,

    [string[]]$AdditionalResourcePaths = @(),

    [bool]$IncludeAssets = $true,

    [bool]$IncludeCompilerRuntime = $true,

    [bool]$NoTranslations = $true,

    [bool]$CreateZip = $true
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$ScriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$ProjectRoot = [System.IO.Path]::GetFullPath((Join-Path -Path $ScriptRoot -ChildPath '..'))

function Get-NormalizedPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw 'Path cannot be empty.'
    }

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }

    return [System.IO.Path]::GetFullPath((Join-Path -Path $ProjectRoot -ChildPath $Path))
}

function Resolve-QtBinDir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedPath = Get-NormalizedPath -Path $Path
    $directExe = Join-Path -Path $resolvedPath -ChildPath 'windeployqt.exe'
    if (Test-Path -LiteralPath $directExe -PathType Leaf) {
        return $resolvedPath
    }

    $binDir = Join-Path -Path $resolvedPath -ChildPath 'bin'
    $binExe = Join-Path -Path $binDir -ChildPath 'windeployqt.exe'
    if (Test-Path -LiteralPath $binExe -PathType Leaf) {
        return $binDir
    }

    throw "Unable to locate windeployqt.exe under QtPath: $resolvedPath"
}

function Find-ExecutableInBuildDir {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,

        [Parameter(Mandatory = $true)]
        [string]$Config,

        [Parameter(Mandatory = $true)]
        [string]$AppName
    )

    $resolvedBuildDir = Get-NormalizedPath -Path $BuildDir
    if (-not (Test-Path -LiteralPath $resolvedBuildDir -PathType Container)) {
        throw "Build directory does not exist: $resolvedBuildDir"
    }

    $candidatePaths = @(
        (Join-Path -Path $resolvedBuildDir -ChildPath "$Config\$AppName.exe"),
        (Join-Path -Path $resolvedBuildDir -ChildPath $AppName) + '.exe',
        (Join-Path -Path $resolvedBuildDir -ChildPath "bin\$Config\$AppName.exe"),
        (Join-Path -Path $resolvedBuildDir -ChildPath "bin\$AppName.exe")
    )

    foreach ($candidate in $candidatePaths) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return [System.IO.Path]::GetFullPath($candidate)
        }
    }

    $tried = $candidatePaths -join [Environment]::NewLine
    throw "Unable to locate $AppName.exe in build directory.$([Environment]::NewLine)Tried:$([Environment]::NewLine)$tried"
}

function Reset-Directory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if (Test-Path -LiteralPath $Path) {
        Remove-Item -LiteralPath $Path -Recurse -Force
    }

    New-Item -ItemType Directory -Path $Path | Out-Null
}

function Copy-PathToDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$SourcePath,

        [Parameter(Mandatory = $true)]
        [string]$DestinationDirectory
    )

    $resolvedSource = Get-NormalizedPath -Path $SourcePath
    if (-not (Test-Path -LiteralPath $resolvedSource)) {
        throw "Resource path does not exist: $resolvedSource"
    }

    $destinationPath = Join-Path -Path $DestinationDirectory -ChildPath (Split-Path -Path $resolvedSource -Leaf)
    Copy-Item -LiteralPath $resolvedSource -Destination $destinationPath -Recurse -Force
}

$qtBinDir = Resolve-QtBinDir -Path $QtPath
$windeployqt = Join-Path -Path $qtBinDir -ChildPath 'windeployqt.exe'

if (-not $PSBoundParameters.ContainsKey('AppName') -or [string]::IsNullOrWhiteSpace($AppName)) {
    if ($PSCmdlet.ParameterSetName -eq 'ByExecutablePath') {
        $AppName = [System.IO.Path]::GetFileNameWithoutExtension($ExecutablePath)
    } else {
        $AppName = 'SweetEditorQtDemo'
    }
}

if ($Config -ne 'Release') {
    Write-Warning "Current config is $Config. For end-user distribution, Release is usually the correct choice."
}

$sourceExe = if ($PSCmdlet.ParameterSetName -eq 'ByExecutablePath') {
    $resolvedExe = Get-NormalizedPath -Path $ExecutablePath
    if (-not (Test-Path -LiteralPath $resolvedExe -PathType Leaf)) {
        throw "Executable path does not exist: $resolvedExe"
    }
    $resolvedExe
} else {
    Find-ExecutableInBuildDir -BuildDir $BuildDir -Config $Config -AppName $AppName
}

$resolvedDistRoot = Get-NormalizedPath -Path $DistRoot
if (-not (Test-Path -LiteralPath $resolvedDistRoot -PathType Container)) {
    New-Item -ItemType Directory -Path $resolvedDistRoot | Out-Null
}

$packageName = if ($Config -eq 'Release') { $AppName } else { "$AppName-$Config" }
$deployDir = Join-Path -Path $resolvedDistRoot -ChildPath $packageName
Reset-Directory -Path $deployDir

$deployExe = Join-Path -Path $deployDir -ChildPath ([System.IO.Path]::GetFileName($sourceExe))
Copy-Item -LiteralPath $sourceExe -Destination $deployExe -Force

$windeployArgs = @()
if ($Config -eq 'Debug') {
    $windeployArgs += '--debug'
} else {
    $windeployArgs += '--release'
}

if ($IncludeCompilerRuntime) {
    $windeployArgs += '--compiler-runtime'
}

if ($NoTranslations) {
    $windeployArgs += '--no-translations'
}

if (-not [string]::IsNullOrWhiteSpace($QmlDir)) {
    $windeployArgs += '--qmldir'
    $windeployArgs += (Get-NormalizedPath -Path $QmlDir)
}

$windeployArgs += '--dir'
$windeployArgs += $deployDir
$windeployArgs += '--verbose'
$windeployArgs += '0'
$windeployArgs += $deployExe

Write-Host "Source exe    : $sourceExe"
Write-Host "Deploy dir    : $deployDir"
Write-Host "Qt bin dir    : $qtBinDir"
Write-Host "windeployqt   : $windeployqt"

& $windeployqt @windeployArgs
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE."
}

if ($IncludeAssets) {
    $assetsDir = Join-Path -Path $ProjectRoot -ChildPath 'assets'
    if (Test-Path -LiteralPath $assetsDir -PathType Container) {
        Copy-PathToDirectory -SourcePath $assetsDir -DestinationDirectory $deployDir
    }
}

foreach ($resourcePath in $AdditionalResourcePaths) {
    Copy-PathToDirectory -SourcePath $resourcePath -DestinationDirectory $deployDir
}

if ($CreateZip) {
    $zipPath = Join-Path -Path $resolvedDistRoot -ChildPath ($packageName + '.zip')
    if (Test-Path -LiteralPath $zipPath -PathType Leaf) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path $deployDir -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Zip package   : $zipPath"
}

Write-Host 'Deployment completed.'
