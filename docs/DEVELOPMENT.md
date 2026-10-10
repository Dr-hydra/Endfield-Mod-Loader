# E Mod Loader 开发与构建

当前维护代码在 `src/`。原样迁入的 `native/`、`ui/`、`android/` 及 `provenance/` 用于核对来源，不是当前产品构建入口。

## Windows

需要 Visual Studio 2022 C++ Build Tools、CMake 3.25+、.NET SDK 9.0.300 或同系列更新补丁，以及用于模块网页的 WebView2 Runtime。

```powershell
./scripts/Build.ps1
```

可以用 `-CMake <cmake.exe>` 和 `-Dotnet <dotnet.exe>` 指定工具。独立、自包含的桌面包生成于 `build/package/windows/`，运行 `EModLoader.exe`。它不需要 BE 安装目录或 BE 用户配置；开发工具可以存放在任意位置。

维护结构：

- `src/EModLoader.Core/`：旧包校验、不可变安装代次、原始配置、宿主连接和旧网页桥协议。
- `src/EModLoader.Desktop/`：WPF 管理界面和 WebView2 模块容器。
- `src/native/runtime/`：原模块装载流程和可选 IL2CPP 辅助接口。
- `src/native/hooks/`：共享 Hook 链与双端 backend。
- `include/BetterEndfield/`：保持原 BE SDK V1 的 C ABI。

数据默认保存于 `%LOCALAPPDATA%/EndfieldModLoader/`。开发测试可通过 `EML_DATA_ROOT` 指定独立目录。原 ZIP 导入后在 EML 新目录创建 UUID 代次，DLL/SO、网页和作者业务字段不修改。卸载/升级时当前可能仍使用的文件保留。

`启动离线宿主` 在独立 EML 测试进程执行模块；`启动游戏` 使用显式启动与标准系统加载路径。两种模式会在界面显示不同连接状态。游戏如果要求管理员权限，需要以足够权限运行加载器；当前开发预览没有自动提升权限。遇到已有 XInput 代理则报告加载方式冲突；不实现 BE/EML 跨产品互斥。

## Android

需要 JDK 17+、Android SDK API 37、NDK 27.2.12479018、SDK CMake 3.22.1 和支持 `DobbyPrepare/DobbyCommit` 的 Dobby 源码。AGP 9.1.1 本机构建使用 JDK 21+。

```powershell
./scripts/BuildAndroid.ps1 -Sdk <SDK路径> -Dobby <Dobby源码路径> -JavaHome <JDK路径>
```

独立包名为 `dev.endfield.modloader`。Debug APK 输出于 `build/android/app/outputs/apk/debug/`。这是开发签名构建，不作为正式签名身份发布。

管理应用支持本地导入、配置、排序和模块网页。离线宿主运行于自己的 `:offline` 进程。游戏接入沿用 libxposed API 102 的目标进程范围、远程文件发布、游戏私有不可变代次和 Unity 首帧触发的 native 加载。仅在框架授权的游戏进程使用原 namespace 接入；如果平台拒绝相关入口，则报告错误并保持未加载。

原生 Host 提供原 BE V1 生命周期、消息、日志与 Hook 链。游戏辅助接口等待 IL2CPP 就绪后可查询；不提供模块的业务正确性或 Unity 主线程安全保证。没有手机或游戏验证，Android 框架接入、实际 linker namespace 和渲染时序标记未验证。

## 离线回归

```powershell
dotnet run --project tests/EModLoader.CoreTests -c Release -- <原EchoZIP> <产品OfflineHost.exe>
dotnet run --project tests/EModLoader.DesktopTests -c Release -- <原EchoZIP> <产品OfflineHost.exe> <输出目录>
ctest --test-dir build/native/windows -C Release --output-on-failure
python tests/verify_launch.py --native-bin build/native/windows/Release
node tests/verify_bridge.js
python tests/android/verify_materializer.py --json-jar <org.json.jar>
```

桌面测试使用实际 WebView2 打开发行包原页面，旧 `window.betterEndfield` 完成配置、状态、发送、回复和事件。启动测试只使用自有受控进程，不启动游戏。Android materializer 使用 JVM 存储适配器，不能称为 APK 实机执行。

第一人称仍位于 `examples/first-person/archive/`，未成为可加载插件。它需要下一步从原相机上下文提取和单独验证。
