param([string]$CMake = 'cmake', [string]$Dotnet = 'dotnet', [string]$Configuration = 'Release', [switch]$SkipNative)
$ErrorActionPreference = 'Stop'
$taskRepoRoot = Split-Path -Parent $PSScriptRoot
$taskNativeBuild = Join-Path $taskRepoRoot 'build/native/windows-compat-test'
$taskPackage = Join-Path $taskRepoRoot "build/package/compat-test/$([Guid]::NewGuid().ToString('N'))"
$taskNativeRuntime = Join-Path $taskNativeBuild $Configuration
if (!$SkipNative) {
    & $CMake -S (Join-Path $taskRepoRoot 'src/native') -B $taskNativeBuild -A x64 -DEML_COMPATIBILITY_TEST_BUILD=ON
    if ($LASTEXITCODE -ne 0) { throw 'Compatibility native configuration failed' }
    & $CMake --build $taskNativeBuild --config $Configuration --parallel 6
    if ($LASTEXITCODE -ne 0) { throw 'Compatibility native build failed' }
}
foreach ($taskFile in @('EModLoader.OfflineHost.exe','EModLoader.Launcher.exe','EModLoader.Host.dll','EModLoader.XInputProxy.dll')) {
    if (!(Test-Path -LiteralPath (Join-Path $taskNativeRuntime $taskFile) -PathType Leaf)) { throw "Missing runtime: $taskFile" }
}
& $Dotnet publish (Join-Path $taskRepoRoot 'src/EModLoader.Desktop/EModLoader.Desktop.csproj') -c $Configuration -r win-x64 --self-contained true -o $taskPackage `
    -p:EMLCompatibilityTest=true -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:EnableCompressionInSingleFile=true `
    -p:DebugType=embedded -p:DebugSymbols=false
if ($LASTEXITCODE -ne 0) { throw 'Compatibility desktop publish failed' }
$taskFiles = @(Get-ChildItem -LiteralPath $taskPackage -Recurse -File)
if ($taskFiles.Count -ne 1 -or $taskFiles[0].Name -ne 'EModLoader.exe') { throw 'Desktop publish produced unexpected external files.' }
Rename-Item -LiteralPath $taskFiles[0].FullName -NewName 'EModLoader-XInput兼容测试.exe'
New-Item -ItemType Directory -Path (Join-Path $taskPackage 'runtime') | Out-Null
foreach ($taskFile in @('EModLoader.OfflineHost.exe','EModLoader.Launcher.exe','EModLoader.Host.dll','EModLoader.XInputProxy.dll')) {
    Copy-Item -LiteralPath (Join-Path $taskNativeRuntime $taskFile) -Destination (Join-Path $taskPackage 'runtime')
}
foreach ($taskDocument in @('LICENSE','THIRD_PARTY_NOTICES.md')) {
    Copy-Item -LiteralPath (Join-Path $taskRepoRoot $taskDocument) -Destination $taskPackage
}
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'docs/XINPUT_COMPATIBILITY_TEST.md') -Destination (Join-Path $taskPackage '使用说明.md')
Write-Output "Self-contained compact package: $taskPackage"
