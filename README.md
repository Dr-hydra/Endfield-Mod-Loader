# Endfield Mod Loader

简称 **E Mod Loader**。独立的终末地第三方模块项目。

当前阶段按原样保存从 Better Endfield Next 迁入的第三方模块实现与第一人称归档，尚未选择游戏加载方式，尚不是可安装运行的独立产品。源码中的旧命名、ABI、配置键与网页协议暂时保留，不代表新产品已经完成身份或代码独立化。

- `native/shared/third_party_modules/`：原生模块 Host、生命周期、配置、消息与日志。
- `ui/BetterEndfieldNext.UI/`：Windows 模块管理与网页容器原始实现。
- `android/app/src/main/`：Android 模块包、管理页、网页桥和运行时资源准备。
- `tools/ThirdPartyModules/`：原 SDK 打包脚本与 Echo 示例。
- `native/tests/`：原第三方模块回归测试。
- `examples/first-person/archive/`：第一人称专用源码、工具、测试及原相机集成快照。它还不是独立可加载的示例模块。
- `provenance/integration/`：从 BE 原样保存的接入文件，用于后续拆分；不参与产品构建。
- [迁移清单](provenance/migration.json)：源提交、原始相对路径、目标路径与 SHA-256。
- [加载方式讨论](docs/LOADING_DESIGN.md)：待决策边界。

本项目采用 AGPL-3.0-only，保留迁入代码来源及依赖许可。原工程中通用 Host、内置模块加载和共享 Hook 链仍由 BE 保留维护。

可用 `python scripts/verify_import.py` 校验原样迁入文件。原打包脚本和平台测试仍依赖 BE 的构建环境与集成文件，现阶段不作为独立构建入口。
