# XInput 兼容性测试版

双击 `EModLoader-XInput兼容测试.exe`，确认游戏路径后点击“启动游戏”。“允许现有 XInput DLL（兼容性测试）”默认勾选，可取消以恢复严格冲突检查。

测试版保留游戏目录中已有的 `xinput1_4.dll`，再向游戏进程加载 EML Host，便于测试 EML 模块与原有加载器共存。原加载器能否正常工作仍取决于它自身的加载逻辑；两个工具若修改同一处游戏逻辑，需要在实际游戏中验证。

已有外来 XInput DLL 时，EML 的安装和卸载 DLL 自启动功能仍拒绝操作，不覆盖现有 DLL。EML 自己的 XInput 代理继续负责自启动，不重复注入 Host。普通版保留原冲突检查。

发布包采用精简目录：自包含主程序、`runtime` 中四个启动组件，以及许可和说明文件。已内置 .NET 运行时，无需另装 .NET；模块网页仍使用系统的 Microsoft Edge WebView2 Runtime。请整体移动目录，保留 `runtime` 子目录。

此测试版沿用原 EML 模块库和保存的游戏路径，也支持原有 `EML_DATA_ROOT` 设置。可导入 `G:\LR本地后端-r17-EML模块.zip` 测试 LR 本地后端模块。切换已加载的模块或加载器后，需重新启动游戏。

重建：在项目目录运行 `scripts/BuildCompatibilityTest.ps1`。兼容测试原生组件与普通版分别构建到 `build/native/windows-compat-test` 和 `build/native/windows`，托管中间文件也分别存放。`PublishSingleFile` 用于合并 .NET 程序集和减少散落文件，整个交付包仍是目录包。
