param([string]$CMake = 'cmake',[string]$Dotnet = 'dotnet',[string]$Configuration = 'Release',[switch]$SkipNative)
$ErrorActionPreference = 'Stop'
$taskRepoRoot = Split-Path -Parent $PSScriptRoot
$taskVersion = ([xml](Get-Content -LiteralPath (Join-Path $taskRepoRoot 'Directory.Build.props') -Raw)).Project.PropertyGroup.Version
$taskNativeBuild = Join-Path $taskRepoRoot "build/native/windows-$taskVersion"
$taskPackage = Join-Path $taskRepoRoot "build/package/windows-$taskVersion"
if (!$SkipNative) {
    & $CMake -S (Join-Path $taskRepoRoot 'src/native') -B $taskNativeBuild -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Native configuration failed' }
    & $CMake --build $taskNativeBuild --config $Configuration --parallel 6
    if ($LASTEXITCODE -ne 0) { throw 'Native build failed' }
}
& $Dotnet publish (Join-Path $taskRepoRoot 'src/EModLoader.Desktop/EModLoader.Desktop.csproj') -c $Configuration -r win-x64 --self-contained true -o $taskPackage -p:PublishSingleFile=true -p:DebugType=embedded -p:DebugSymbols=false -p:CopyOutputSymbolsToPublishDirectory=false
if ($LASTEXITCODE -ne 0) { throw 'Desktop publish failed' }
New-Item -ItemType Directory -Path (Join-Path $taskPackage 'runtime') -Force | Out-Null
foreach ($taskFileName in @('EModLoader.OfflineHost.exe','EModLoader.Launcher.exe','EModLoader.Host.dll','EModLoader.XInputProxy.dll')) {
    Copy-Item -LiteralPath (Join-Path $taskNativeBuild "$Configuration/$taskFileName") -Destination (Join-Path $taskPackage 'runtime') -Force
}
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'LICENSE') -Destination $taskPackage -Force
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'THIRD_PARTY_NOTICES.md') -Destination $taskPackage -Force
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'README.md') -Destination $taskPackage -Force
New-Item -ItemType Directory -Path (Join-Path $taskPackage 'docs') -Force | Out-Null
foreach($taskObsolete in @('IMPLEMENTATION_0_1.md','COMPATIBILITY_PLAN.md','FIRST_PERSON_MODULE.md','LOADING_DESIGN.md','LOADING_RESEARCH.md','OFFLINE_VALIDATION.md')) {
    $taskOldDocument=Join-Path $taskPackage "docs/$taskObsolete"
    if(Test-Path -LiteralPath $taskOldDocument -PathType Leaf){Remove-Item -LiteralPath $taskOldDocument}
}
foreach($taskDocument in @('USER_GUIDE.md','DEVELOPMENT.md','SDK.md','COMPATIBILITY.md','XINPUT_COEXISTENCE.md','VALIDATION_1_6_3.md')) {
    Copy-Item -LiteralPath (Join-Path $taskRepoRoot "docs/$taskDocument") -Destination (Join-Path $taskPackage 'docs') -Force
}
$taskExampleDocs=Join-Path $taskPackage 'examples/first-person'
New-Item -ItemType Directory -Path $taskExampleDocs -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $taskRepoRoot 'examples/first-person/README.md') -Destination $taskExampleDocs -Force
Write-Output "Built standalone package: $taskPackage"
