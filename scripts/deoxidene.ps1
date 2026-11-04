#!/usr/bin/env pwsh
param(
    [Parameter(Position = 0)] [string]$Command,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]]$Rest
)

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Show-Usage {
    @"
deoxidene - cargo-equivalent wrapper over CMakePresets

Usage:
  deoxidene.ps1 new <name>          Scaffold a new consumer project from this template
  deoxidene.ps1 build [preset]      Configure + build (default preset: release)
  deoxidene.ps1 run [preset] [args] Build then run the hello example
  deoxidene.ps1 test [preset]       Build then run the CTest suite (default preset: debug)
  deoxidene.ps1 test-sanitize       Build + run under asan-ubsan preset
  deoxidene.ps1 tidy                Run clang-tidy over include/
  deoxidene.ps1 bench               Build native-perf (LTO, host CPU tuning) and run the benchmark
  deoxidene.ps1 pgo                 Profile-guided build of native-perf, trained on the benchmark (GCC only)
"@
}

function Invoke-New {
    param([string]$Name)
    if (-not $Name) { throw "usage: deoxidene.ps1 new <name>" }
    New-Item -ItemType Directory -Force -Path $Name | Out-Null
    foreach ($item in @('CMakeLists.txt','CMakePresets.json','.clang-tidy','include','third_party','tests')) {
        Copy-Item -Recurse -Force (Join-Path $RepoRoot $item) $Name
    }
    New-Item -ItemType Directory -Force -Path (Join-Path $Name 'src') | Out-Null
    Write-Host "Scaffolded $Name from deoxidene template."
}

function Invoke-Build {
    param([string]$Preset = 'release')
    cmake --preset $Preset -S $RepoRoot -B (Join-Path $RepoRoot "build/$Preset")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build (Join-Path $RepoRoot "build/$Preset")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Run {
    param([string]$Preset = 'release', [string[]]$Args)
    Invoke-Build -Preset $Preset
    & (Join-Path $RepoRoot "build/$Preset/examples/hello/deoxidene_hello") @Args
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Test {
    param([string]$Preset = 'debug')
    Invoke-Build -Preset $Preset
    ctest --test-dir (Join-Path $RepoRoot "build/$Preset") --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-TestSanitize {
    Invoke-Build -Preset 'asan-ubsan'
    & (Join-Path $RepoRoot "build/asan-ubsan/examples/hello/deoxidene_hello")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Tidy {
    Invoke-Build -Preset 'debug'
    $Headers = @(Get-ChildItem -Path (Join-Path $RepoRoot 'include/deoxidene') -Filter '*.hpp' | ForEach-Object { $_.FullName })
    clang-tidy -p (Join-Path $RepoRoot 'build/debug') @Headers '--warnings-as-errors=*'
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Bench {
    Invoke-Build -Preset 'native-perf'
    & (Join-Path $RepoRoot "build/native-perf/bench/deoxidene_bench")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Invoke-Pgo {
    $ProfileDir = Join-Path $RepoRoot 'build/pgo-profile'
    $BuildDir = Join-Path $RepoRoot 'build/pgo'
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $ProfileDir, $BuildDir
    cmake --preset native-perf -S $RepoRoot -B $BuildDir "-DCMAKE_CXX_FLAGS=-fprofile-generate=$ProfileDir"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build $BuildDir --target deoxidene_bench
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $BuildDir 'bench/deoxidene_bench') | Out-Null
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --preset native-perf -S $RepoRoot -B $BuildDir "-DCMAKE_CXX_FLAGS=-fprofile-use=$ProfileDir -fprofile-partial-training -Wno-missing-profile"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build $BuildDir --target deoxidene_bench
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & (Join-Path $BuildDir 'bench/deoxidene_bench')
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

function Get-Preset {
    param([string]$Default)
    if ($FirstArg) { return $FirstArg }
    return $Default
}

$FirstArg = if ($Rest -and $Rest.Length -gt 0) { $Rest[0] } else { $null }
$RunArgs = if ($Rest -and $Rest.Length -gt 1) { $Rest[1..($Rest.Length - 1)] } else { @() }

switch ($Command) {
    'new'           { Invoke-New -Name $FirstArg }
    'build'         { Invoke-Build -Preset (Get-Preset 'release') }
    'run'           { Invoke-Run -Preset (Get-Preset 'release') -Args $RunArgs }
    'test'          { Invoke-Test -Preset (Get-Preset 'debug') }
    'test-sanitize' { Invoke-TestSanitize }
    'tidy'          { Invoke-Tidy }
    'bench'         { Invoke-Bench }
    'pgo'           { Invoke-Pgo }
    default         { Show-Usage; exit 1 }
}
