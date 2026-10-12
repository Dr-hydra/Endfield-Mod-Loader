# Legacy entry point: XInput coexistence is a saved option in the regular build since 0.1.2.
param([string]$CMake = 'cmake', [string]$Dotnet = 'dotnet', [string]$Configuration = 'Release', [switch]$SkipNative)
& (Join-Path $PSScriptRoot 'Build.ps1') -CMake $CMake -Dotnet $Dotnet -Configuration $Configuration -SkipNative:$SkipNative
