param([Parameter(Mandatory=$true)][string]$Sdk,[Parameter(Mandatory=$true)][string]$Dobby,[Parameter(Mandatory=$true)][string]$JavaHome)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskProject=Join-Path $taskRoot 'src/android'
$env:JAVA_HOME=$JavaHome
$env:ANDROID_HOME=$Sdk
$env:ANDROID_SDK_ROOT=$Sdk
$env:Path=(Join-Path $JavaHome 'bin')+';'+$env:Path
& (Join-Path $taskProject 'gradlew.bat') -p $taskProject :app:assembleDebug :app:lintDebug --no-daemon "-PdobbySource=$Dobby"
if($LASTEXITCODE -ne 0){throw 'Android build failed'}
