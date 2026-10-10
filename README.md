# Endfield Mod Loader

简称 **E Mod Loader**。独立的终末地第三方模块项目。

当前为 **0.1.0 开发预览**。独立 Windows 管理应用、模块 Host、原 BE V1 包兼容、网页容器和离线闭环已实现，Logo 使用用户提供的原图。Windows 显式游戏启动路径已用自有测试进程验证；Android 提供独立管理应用、离线宿主进程和 libxposed 接入。第一人称已拆成 [独立示例模块](examples/first-person/README.md)，支持双端构建和包内配置网页。没有游戏或手机实机验证条件，相关效果标记未验证。

## 使用与构建

Windows 开发包位于 `build/package/windows/`，解压完整分发包后运行 `EModLoader.exe`。选择“导入 ZIP”，启用模块，启动离线宿主，再打开模块界面。旧发行模块的 ZIP、DLL 和网页无需修改；离线宿主与游戏连接在界面分别显示。

Windows 数据目录为 `%LOCALAPPDATA%/EndfieldModLoader/`，与 BE 分开。桌面包自包含 .NET，模块网页需要系统 WebView2 Runtime。Android 包名为 `dev.endfield.modloader`；开发 APK 构建见 [开发说明](docs/DEVELOPMENT.md)。不实现与 BE 的跨产品互斥，也不承诺同时使用兼容。

```powershell
./scripts/Build.ps1
```

编译工具、Android 构建与离线回归命令见 [开发说明](docs/DEVELOPMENT.md)。模块作者继续使用 [兼容 SDK](docs/SDK.md)。

## 维护目录与来源

当前产品代码在 `src/`，公开兼容头文件在 `include/BetterEndfield/`。以下迁入目录保留原始文件和 SHA-256，用于来源追溯，不参与当前产品构建：

- `native/shared/third_party_modules/`：原生模块 Host、生命周期、配置、消息与日志。
- `ui/BetterEndfieldNext.UI/`：Windows 模块管理与网页容器原始实现。
- `android/app/src/main/`：Android 模块包、管理页、网页桥和运行时资源准备。
- `tools/ThirdPartyModules/`：原 SDK 打包脚本与 Echo 示例。
- `native/tests/`：原第三方模块回归测试。
- `examples/first-person/`：独立第一人称示例模块、配置页面、构建与回归；`archive/` 保留原专用源码、工具、测试及相机集成快照。
- `provenance/integration/`：从 BE 原样保存的接入文件，用于后续拆分；不参与产品构建。
- [迁移清单](provenance/migration.json)：源提交、原始相对路径、目标路径与 SHA-256。
- [加载方式讨论](docs/LOADING_DESIGN.md)：待决策边界。
- [加载机制调研与开发建议](docs/LOADING_RESEARCH.md)：原加载核心已通过独立 Windows/Echo 验证；复用范围、命名/ABI 改造、自身初始化保护和第一人称拆分建议。按最新选择不实现与 BE 的跨产品互斥。
- [已发行模块包兼容计划](docs/COMPATIBILITY_PLAN.md)：原 BE format 1 / ABI 1 包无需修改或重新编译；Next 不在兼容范围内。当前仍处于原型验证阶段。
- [离线开发与验收计划](docs/OFFLINE_VALIDATION.md)：当前没有实机游戏验证条件，使用原发行包、离线宿主、受控测试进程和模拟回归继续开发；游戏内效果单独标记未验证。

本项目采用 AGPL-3.0-only，保留迁入代码来源及依赖许可。原工程中通用 Host、内置模块加载和共享 Hook 链仍由 BE 保留维护。

可用 `python scripts/verify_import.py` 校验原样迁入文件。原迁入打包脚本和平台测试仍作为来源参考；正式构建使用 `scripts/Build.ps1` 和 `scripts/BuildAndroid.ps1`，实际验证结果见 [0.1.0 实施记录](docs/IMPLEMENTATION_0_1.md)。
