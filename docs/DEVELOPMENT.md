# 构建与开发

## Windows

需要 Visual Studio 2022 C++ Build Tools、CMake 3.25+ 和 .NET SDK 9.0.300 或更新的 9.0 补丁。运行模块网页需要 WebView2 Runtime。

```powershell
./scripts/Build.ps1
```

可使用 `-CMake <cmake.exe>`、`-Dotnet <dotnet.exe>` 指定工具。当前自包含桌面分发目录为 `build/package/windows-0.1.3/`，原生构建目录为 `build/native/windows-0.1.3/`，入口为 `EModLoader.exe`。主程序合并 .NET 程序集，EML 启动组件集中放在 `runtime/`；不分发 PDB 与 WebView2 XML 文档。`scripts/BuildCompatibilityTest.ps1` 保留为普通构建的旧入口。

## Android

需要 JDK 21+、Android SDK API 37、NDK 27.2.12479018、SDK CMake 3.22.1，以及支持 `DobbyPrepare/DobbyCommit` 的 Dobby 源码。

```powershell
./scripts/BuildAndroid.ps1 -Sdk <SDK路径> -Dobby <Dobby源码路径> -JavaHome <JDK路径>
```

Debug APK 输出于 `build/android/app/outputs/apk/debug/`。应用 ID 为 `dev.endfield.modloader`，最低 Android 版本为 10，游戏接入使用 libxposed API 102。Debug APK 使用开发签名。

## 示例模块

Echo 展示不依赖游戏的配置、消息与事件：

```powershell
cmake -S examples/echo -B build/examples/echo -A x64
cmake --build build/examples/echo --config Release
```

第一人称包含双端库、配置页面和独立回归：

```powershell
./scripts/BuildFirstPerson.ps1 -CMake <cmake.exe>
# 需要 Android SO 时添加 -AndroidSdk <SDK路径>
python scripts/PackageFirstPerson.py --android build/examples/first-person-android/package
```

模块包输出于 `releases/first-person/0.1.0-dev/`。仅构建 Windows 时不传 `--android`。模块说明见 [第一人称](../examples/first-person/README.md)，接口见 [SDK](SDK.md)。

## 代码目录

| 目录 | 内容 |
| --- | --- |
| `src/EModLoader.Core/` | 模块包、安装代次、配置、宿主连接与网页桥 |
| `src/EModLoader.Desktop/` | WPF 管理应用与 WebView2 容器 |
| `src/native/` | Windows 宿主、启动组件、游戏辅助接口与 Hook 链 |
| `src/native/proxy/` | XInput DLL 转发、启动及归属校验 |
| `src/android/` | Android 应用、资源准备、离线宿主与框架入口 |
| `include/BetterEndfield/` | 原 BE V1 兼容 SDK |
| `examples/` | Echo 和第一人称示例 |

仓库中的原始来源快照用于许可和来源核对，不是产品构建入口。`scripts/verify_import.py` 检查来源文件的 SHA-256；参考资料不定义当前 EML 接口。

## 验证

先运行 Windows 构建，再显式编译受控启动和官方父进程验证目标。这两个目标设置为 `EXCLUDE_FROM_ALL`，不会随普通构建生成，也不会进入产品安装包：

```powershell
cmake --build build/native/windows-0.1.3 --config Release --target EMLControlledLauncher EMLOfficialParentTests
build/native/windows-0.1.3/Release/EMLOfficialParentTests.exe <空的测试目录>
dotnet run --project tests/EModLoader.CoreTests -c Release -- <EchoZIP> <OfflineHost.exe>
dotnet run --project tests/EModLoader.DesktopTests -c Release -- <EchoZIP> <OfflineHost.exe> <输出目录>
dotnet run --project tests/EModLoader.DesktopTests -c Release -- <第一人称ZIP> <OfflineHost.exe> <输出目录> first-person
ctest --test-dir build/native/windows-0.1.3 -C Release --output-on-failure
ctest --test-dir build/examples/first-person-windows -C Release --output-on-failure
python tests/verify_launch.py --native-bin build/native/windows-0.1.3/Release
python tests/verify_proxy.py --native-bin build/native/windows-0.1.3/Release
python tests/verify_xinput_compatibility.py --native-bin build/native/windows-0.1.3/Release
dotnet run --project tests/EModLoader.LaunchTests -c Release -- build/native/windows-0.1.3/Release
node tests/verify_bridge.js
python tests/android/verify_materializer.py --json-jar <org.json.jar>
```

开发测试可用 `EML_DATA_ROOT` 指定独立数据目录。桌面测试运行实际 WebView2、路径保存及 DLL 部署；启动测试只使用自有受控进程，覆盖注入器和 DLL 自启动。`verify_launch.py`、`verify_proxy.py` 与 `EModLoader.LaunchTests` 仅在临时测试 runtime 内将 `EMLControlledLauncher.exe` 复制为 `EModLoader.Launcher.exe`，实际产品启动器始终保留官方启动器发现与签名验证。部署和 XInput 冲突保护仍使用产品启动器。提权决策与取消处理使用模拟结果，未执行交互式 UAC。Android 资源准备使用 JVM 适配器；当前测试不覆盖真实游戏和 Android 设备运行。
