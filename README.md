# Endfield Mod Loader

**E Mod Loader（EML）** 是《明日方舟：终末地》的独立模块加载器，支持 Windows x64 和 Android ARM64。它提供模块包导入、启停、配置、日志和模块网页，并兼容原 Better Endfield format 1 / ABI 1 模块包。

[下载开发预览](https://github.com/Dr-hydra/Endfield-Mod-Loader/releases/latest) · [使用指南](docs/USER_GUIDE.md) · [第一人称模块](examples/first-person/README.md) · [模块作者 SDK](docs/SDK.md) · [构建说明](docs/DEVELOPMENT.md)

Windows 当前版本为 **0.1.3-dev**，适配新版客户端的官方父进程启动检查：自动发现并验证官方启动器，保留按需 UAC、XInput 共存、游戏路径记忆与 DLL 自启动。已在 Windows 1.6.3 验证真实游戏启动、Host 通信和第一人称初始化；场景效果与 Android 设备运行仍待实测，详见 [验证记录](docs/VALIDATION_1_6_3.md)。Android 及第一人称模块保持 **0.1.0-dev**。

## 下载与安装

| 附件 | 用途 |
| --- | --- |
| `EModLoader-0.1.3-dev-win-x64.zip` | Windows 加载器；完整解压后运行 `EModLoader.exe` |
| `EModLoader-0.1.0-dev-android-arm64.apk` | Android 管理应用及 libxposed 接入；当前使用开发签名 |
| `example.first-person-0.1.0-dev.zip` | 第一人称模块，包含双端原生库与配置页面；在 EML 中直接导入 ZIP |
| `EModLoader-SDK-0.1.0.zip` | 兼容头文件、Echo / 第一人称示例源码及构建脚本 |

Windows 需要 Windows 10/11 x64 和 WebView2 Runtime。Android 需要 Android 10 及以上、ARM64，以及支持 libxposed API 102 的兼容框架；安装 APK 后还需在框架中启用模块并选择游戏作用域。

## 开始使用

1. 安装或解压对应平台的加载器。
2. 导入作者提供的模块 ZIP，检查平台支持情况并启用模块。
3. 打开模块页面，保存所需配置。
4. Windows 使用 EML 的“启动游戏”，或安装“DLL 自启动”后从官方启动器启动；Android 启用框架作用域后彻底停止并重新启动游戏。

Windows 优先读取已保存的游戏路径，并从注册表、常见安装目录查找 `Endfield.exe`。找到多个安装时需要选择；手动选择后自动保存。需要管理员权限时，启动或部署组件会请求 Windows UAC 确认。DLL 自启动的安装和卸载要求游戏关闭，不覆盖其他工具的同名 DLL。

游戏目录已有其他工具的 `xinput1_4.dll` 时，可勾选“保留其他 XInput DLL 并加载 EML”，再点击“启动游戏”。开关默认关闭，选择会自动保存；开启后保留原 DLL，并额外注入 EML Host。详见 [XInput 共存设置](docs/XINPUT_COEXISTENCE.md)。

新导入的模块默认停用。离线宿主可以检查配置与消息通信，游戏效果需要实际游戏宿主。更新已加载的原生模块后需要重启宿主或游戏。

## 第一人称模块

独立第一人称模块跟随当前角色头部，保留游戏视角输入，支持视野、头部隐藏、阴影保留、颈口填补及静止时转身参数。

导入 `example.first-person-0.1.0-dev.zip` 后启用模块。默认不切换视角：Windows 在游戏窗口获得焦点后按减号 `-` 切换；Android 可在模块页面保存“默认进入第一人称”。Android 暂无游戏内触控按钮。详细参数及限制见 [模块说明](examples/first-person/README.md)。

## 模块与兼容性

原 BE format 1 / ABI 1 包可以直接导入，保留原生导出和网页接口，无需修改 ZIP、重新编译 DLL / SO 或更新网页脚本。具体模块仍需适配其使用的游戏版本和平台；Next 接口不在支持范围内。详见 [兼容性说明](docs/COMPATIBILITY.md)。

模块在宿主进程中执行原生代码，请选择可信作者提供的包。EML 不提供模块进程隔离，也不保证与 BE 同时使用的兼容性。

## 开发与许可

Windows、Android 和示例模块的构建命令见 [构建说明](docs/DEVELOPMENT.md)。模块开发从 [SDK](docs/SDK.md) 与 `examples/echo/` 开始。

本项目采用 **AGPL-3.0-only**。第三方模块接口与第一人称实现源自 Better Endfield，来源与依赖许可见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
