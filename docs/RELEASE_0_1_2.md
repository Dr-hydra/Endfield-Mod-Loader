# E Mod Loader 0.1.2-dev

Windows 普通版新增“保留其他 XInput DLL 并加载 EML”开关。游戏目录已有其他工具的 `xinput1_4.dll` 时，开启后保留原 DLL，并额外加载 EML Host。

- 开关默认关闭，选择自动保存，重启界面后恢复；更换游戏路径保留选择，旧设置可继续读取。
- DLL 自启动的安装和卸载继续保护其他工具的文件；EML 自己的代理避免重复加载 Host。
- Windows 发布包使用自包含精简目录，内置 .NET，四个启动组件集中放在 `runtime`；无需安装 .NET。
- Android、SDK ABI 和第一人称模块本次没有接口改动。

完整解压 Windows ZIP，运行 `EModLoader.exe`。模块网页需要系统 WebView2 Runtime。

验证覆盖开关保存、旧设置、非法设置、游戏路径切换、受控进程中的双加载器共存、DLL 文件保留、安装与卸载保护，以及发布包界面和离线宿主。此前兼容测试版已由用户确认可用；本次更新未自动操作真实游戏。
