[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('x64', 'Win32')]
    [string]$Platform = 'x64',

    [ValidateSet('App', 'Tests', 'All')]
    [string]$Target = 'App',

    [string]$SourceRoot = 'C:\src',
    [string]$OutputRoot = 'C:\out'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $SourceRoot -PathType Container)) {
    throw "Source root does not exist: $SourceRoot"
}

$solution = Join-Path $SourceRoot 'DATura.slnx'
if (-not (Test-Path -LiteralPath $solution -PathType Leaf)) {
    throw "DATura.slnx was not found under the source root: $SourceRoot"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

function Invoke-DATuraBuild {
    param(
        [Parameter(Mandatory)]
        [string]$Project,

        [Parameter(Mandatory)]
        [string]$OutputName
    )

    $projectOutput = Join-Path $OutputRoot $OutputName
    $binaryOutput = Join-Path $projectOutput 'bin'
    $intermediateOutput = Join-Path $projectOutput 'obj'
    New-Item -ItemType Directory -Force -Path $binaryOutput, $intermediateOutput | Out-Null

    Write-Host "Building $Project ($Configuration|$Platform)"
    & msbuild.exe $Project `
        /m `
        /nologo `
        /v:minimal `
        /p:Configuration=$Configuration `
        /p:Platform=$Platform `
        "/p:OutDir=$binaryOutput\" `
        "/p:IntDir=$intermediateOutput\"

    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed for $Project with exit code $LASTEXITCODE"
    }
}

if ($Target -in @('App', 'All')) {
    Invoke-DATuraBuild -Project $solution -OutputName 'app'
}

if ($Target -in @('Tests', 'All')) {
    $testRoot = Join-Path $SourceRoot 'tests'
    $testProjects = Get-ChildItem -LiteralPath $testRoot -Filter '*.vcxproj' -File |
        Sort-Object -Property Name

    foreach ($testProject in $testProjects) {
        $name = [System.IO.Path]::GetFileNameWithoutExtension($testProject.Name)
        Invoke-DATuraBuild -Project $testProject.FullName -OutputName (Join-Path 'tests' $name)
    }
}

Write-Host "Build outputs are available under $OutputRoot"
