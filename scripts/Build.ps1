param([string]$CMake = 'cmake',[string]$Dotnet = 'dotnet',[string]$Configuration = 'Release',[switch]$SkipNative)
$ErrorActionPreference = 'Stop'
$taskRepoRoot = Split-Path -Parent $PSScriptRoot
$taskNativeBuild = Join-Path $taskRepoRoot 'build/native/windows'
$taskPackage = Join-Path $taskRepoRoot 'build/package/windows'
if (!$SkipNative) {
    & $CMake -S (Join-Path $taskRepoRoot 'src/native') -B $taskNativeBuild -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Native configuration failed' }
    & $CMake --build $taskNativeBuild --config $Configuration --parallel 6
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
}
& $Dotnet publish (Join-Path $taskRepoRoot 'src/EModLoader.Desktop/EModLoader.Desktop.csproj') -c $Configuration -r win-x64 --self-contained true -o $taskPackage -p:PublishSingleFile=false
if ($LASTEXITCODE -ne 0) { throw 'Desktop publish failed' }
New-Item -ItemType Directory -Path (Join-Path $taskPackage 'runtime') -Force | Out-Null
foreach ($taskFileName in @('EModLoader.OfflineHost.exe','EModLoader.Launcher.exe','EModLoader.Host.dll')) {
    Copy-Item -LiteralPath (Join-Path $taskNativeBuild "$Configuration/$taskFileName") -Destination (Join-Path $taskPackage 'runtime') -Force
}
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'LICENSE') -Destination $taskPackage -Force
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'THIRD_PARTY_NOTICES.md') -Destination $taskPackage -Force
New-Item -ItemType Directory -Path (Join-Path $taskPackage 'docs') -Force | Out-Null
foreach($taskDocument in @('DEVELOPMENT.md','IMPLEMENTATION_0_1.md','SDK.md','COMPATIBILITY_PLAN.md')) {
    Copy-Item -LiteralPath (Join-Path $taskRepoRoot "docs/$taskDocument") -Destination (Join-Path $taskPackage 'docs') -Force
}
Write-Output "Built standalone package: $taskPackage"
