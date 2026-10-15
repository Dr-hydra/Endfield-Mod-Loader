# E Mod Loader 0.1.4-dev · 多启动器支持与启动选项更新

Windows 支持多官方启动器选择（鹰角国服、Bilibili服、全球服 Gryphline、Epic 渠道、自定义启动器）以及无启动器直接启动（Bypass Launcher）模式，并修复主窗口关闭时的并发重入异常。

- **多启动器支持**：自动扫描并识别主流官方启动器（鹰角网络 CN、Bilibili CN、Gryphline 全球服、Epic 渠道等），满足多版本客户端的官方父进程检查。
- **无启动器模式（Bypass）**：支持脱离官方启动器独立运行，无需安装或启动官方启动器桥接进程。
- **交互与配置记忆**：界面新增启动器下拉选项与浏览选择，与游戏路径智能关联，选择自动保存至 `launcher.json`。
- **稳定性修复**：解决窗口关闭时异步资源释放导致 WPF `InvalidOperationException`（重入关闭）的报错弹窗。
- **兼容性保持**：保留按需 UAC 提权、XInput 共存选项、游戏路径记忆与 DLL 自启动功能，不破坏现有模块 ABI 与通信协议。

下载 `EModLoader-0.1.4-dev-win-x64.zip`，完整解压后运行 `EModLoader.exe`；需要系统 WebView2 Runtime。Android `0.1.0-dev`、第一人称模块 `0.1.0-dev` 附件沿用原文件，SDK ABI 保持为 1、SDK 保持为 `0.1.0`。
