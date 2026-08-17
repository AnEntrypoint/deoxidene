#!/usr/bin/env pwsh
param(
    [Parameter(Position = 0)] [string]$Command,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)] [string[]]$Rest
)

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Show-Usage {
    @"
deoxidene - cargo-equivalent wrapper over CMakePresets + vcpkg

Usage:
  deoxidene.ps1 new <name>          Scaffold a new consumer project from this template
  deoxidene.ps1 build [preset]      Configure + build (default preset: release)
  deoxidene.ps1 run [preset] [args] Build then run the hello example
  deoxidene.ps1 test-sanitize       Build + run under asan-ubsan preset
  deoxidene.ps1 tidy                Run clang-tidy over include/
"@
}

function Invoke-New {
    param([string]$Name)
    if (-not $Name) { throw "usage: deoxidene.ps1 new <name>" }
    New-Item -ItemType Directory -Force -Path $Name | Out-Null
    foreach ($item in @('CMakeLists.txt','CMakePresets.json','vcpkg.json','vcpkg-configuration.json','.clang-tidy','cmake','vcpkg-triplets','include')) {
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
}

function Invoke-TestSanitize {
    Invoke-Build -Preset 'asan-ubsan'
    & (Join-Path $RepoRoot "build/asan-ubsan/examples/hello/deoxidene_hello")
}

function Invoke-Tidy {
    Invoke-Build -Preset 'debug'
    clang-tidy -p (Join-Path $RepoRoot "build/debug") (Join-Path $RepoRoot "include/deoxidene/*.hpp")
}

switch ($Command) {
    'new'           { Invoke-New -Name $Rest[0] }
    'build'         { Invoke-Build -Preset ($Rest[0] ?? 'release') }
    'run'           { Invoke-Run -Preset ($Rest[0] ?? 'release') -Args ($Rest[1..($Rest.Length-1)]) }
    'test-sanitize' { Invoke-TestSanitize }
    'tidy'          { Invoke-Tidy }
    default         { Show-Usage; exit 1 }
}
