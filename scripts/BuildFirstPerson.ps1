param(
    [string]$CMake='cmake',
    [string]$AndroidSdk='',
    [string]$NdkVersion='27.2.12479018'
)
$ErrorActionPreference='Stop'
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskSource=Join-Path $taskRoot 'examples/first-person'
$taskWindows=Join-Path $taskRoot 'build/examples/first-person-windows'
& $CMake -S $taskSource -B $taskWindows -A x64
if($LASTEXITCODE -ne 0){throw 'Windows example configuration failed'}
& $CMake --build $taskWindows --config Release
if($LASTEXITCODE -ne 0){throw 'Windows example build failed'}
$taskCTest=Join-Path (Split-Path -Parent (Get-Command $CMake).Source) 'ctest.exe'
& $taskCTest --test-dir $taskWindows -C Release --output-on-failure
if($LASTEXITCODE -ne 0){throw 'First-person regression failed'}
if($AndroidSdk){
    $taskAndroid=Join-Path $taskRoot 'build/examples/first-person-android'
    $taskToolchain=Join-Path $AndroidSdk "ndk/$NdkVersion/build/cmake/android.toolchain.cmake"
    $taskNinja=Join-Path $AndroidSdk 'cmake/3.22.1/bin/ninja.exe'
    & $CMake -S $taskSource -B $taskAndroid -G Ninja "-DCMAKE_TOOLCHAIN_FILE=$taskToolchain" "-DCMAKE_MAKE_PROGRAM=$taskNinja" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DANDROID_STL=c++_static -DCMAKE_BUILD_TYPE=Release
    if($LASTEXITCODE -ne 0){throw 'Android example configuration failed'}
    & $CMake --build $taskAndroid
    if($LASTEXITCODE -ne 0){throw 'Android example build failed'}
}
