# EML 新版本验证记录

日期：2026-10-15。工作区：`G:\Endfield Mod Loader`；参考实现：`G:\Better Endfield`。Windows 实机为 `G:\Arknights Endfield` 的 CN 1.6.3 / Unity 2021.3.34f5；Android 使用本机已有 1.6.4 / versionCode 61 / ARM64 输入。本轮未计算产物哈希。Windows 修复随 0.1.3-dev 发布，Android 和第一人称模块维持原版本。

## 结论

Windows 新版直接启动、EML Host 装载与通信、Echo 以及第一人称运行时初始化有效。第一人称的必要方法、字段和结构布局在 Windows 1.6.3 与 Android 1.6.4 均匹配，不需要调整示例生产代码。

用户本轮暂不进入可操作场景，因此视点、输入、角色切换、头部隐藏、颈口填补、阴影及停用恢复尚未验收。Android 完成示例构建和静态审计，未运行设备上的 EML。角色资料仍覆盖原有 33 个角色，新角色祀没有专用头饰/局部裁剪规则，需单独验证。

## 直接启动改动

原工作区的临时实现只在 `globalgamemanagers` 中发现 `1.6.3` 时读取手写 `official_parent`，原生层也未验证父文件。现已参照 BE 的正式 `native/loaders/shared/official_launcher.{h,cpp}` 接入：

- 自动从官方协议、安装项及安装目录发现 `Games.exe`；验证 Windows 信任签名及鹰角发行组织，验证至创建进程期间保持文件锁。
- `official_parent` 保留为可选覆盖，指定文件同样必须通过验证；去除游戏版本字符串门槛。
- 创建暂停官方父进程，使用 Windows 父进程属性创建游戏；父主线程不恢复，桥接进程等待游戏退出后清理自身父进程。
- 管道及时返回游戏 PID，保留 Win32 `740` 以触发现有按需 UAC；保留 XInput 共存设置。
- 创建、恢复或注入失败时清理本次未完成启动的游戏和父进程。

受控任意父进程仅存在于独立 `EMLControlledLauncher` 测试二进制，生产 Launcher 没有对应开关；测试二进制不进入分发目录。

## 验证结果

| 项目 | 结果 |
| --- | --- |
| Windows 主程序与原生组件 | Release 构建、独立分发目录生成通过 |
| 官方启动器发现、验签与参数 | 16 项通过；本机自动发现有效官方文件 |
| C# 启动链 | 59 项通过，含即时 IPC、提权策略、拒绝伪父、父子生命周期和失败清理 |
| Host native CTest | 2/2 通过 |
| 受控直接启动 / DLL 自启动 / XInput | 通过；代理 16 项、共存 7 项 |
| Echo CoreTests | 19 项通过 |
| Echo / 第一人称 WebView2 | 两组通过，含消息、配置、事件和生命周期 |
| Windows / Android 网页桥 | 通过 |
| Echo / 第一人称双端示例 | Windows x64 与 Android ARM64 均重新编译通过 |
| 第一人称 native 回归 | 6/6 通过 |
| Windows Host 与第一人称所需 IL2CPP 导出 | 30/30 存在 |
| 两平台第一人称方法 | 各 80 个签名和非零入口匹配；2 个可选 Mesh 接口裁剪，由既有回退覆盖 |
| 两平台字段与结构布局 | 各 14/14 字段、20/20 布局字段通过 |

另修复 `DesktopTests` 未声明自包含导致 `NETSDK1151` 的工程属性问题；文档中的默认命令现已实际通过。

## Windows 真实游戏证据

最终测试未传 `--official-parent`，生产 Launcher 自动发现 `D:\Hypergryph Launcher\1.6.0\Games.exe`，通过正常 Windows UAC 启动。进程链为 Launcher `30336` → Games `10268` → Endfield `6068`。

游戏自身 `OneChannelLC.log` 于 **10:36:45.170** 记录 `Parent process check success:10268`，检查结果内层 `code:0`。测试没有修改游戏 DLL，也没有安装 XInput 代理。

Host 记录 `Optional engine helpers ready`，Echo 返回 `runtime_helper_ready=true` 并完成消息往返，第一人称报告 `ready`。CameraState 解析结果为 `header=0x10, rawPos=0xA4, rawRot=0xB0, lens=0x44`。仅缺失可选 `fp.mesh.submesh` 和 `fp.mesh.index_start`，与静态审计一致。

本次采集含 **22 个就绪样本**，未激活第一人称。游戏随后以退出码 **0** 结束，桥接和官方父进程均无残留；退出后的 Host 断开符合已观察到的退出状态。该结果证明启动、初始化及退出链，不能代替地图内效果验证。

采集脚本最初将“最后样本连接已断开”统一记为 `INCOMPLETE`。现已修正分类：先前初始化成功且明确观察到退出码 0 可记为初始化通过；非零退出、无初始化或不明断开仍不能通过。原采集 JSON 保留，修正后的 `summary.json` 根据同一批原始样本生成。

## 证据与复跑

下列路径均相对源码工作区；测试认证令牌仅在隔离模块索引中，不写入报告：

- `build/validation-163/game-final/`：运行命令、原始采集与摘要；`modules/host.log` 为真实 Host 日志。
- `research/compatibility-1.6.3/windows-game-evidence.json`：本轮启动、父校验、代表性模块回复和退出的精简记录。
- `build/validation-163/host-regressions/REPORT.md`：Host、WebView2 与受控启动完整日志索引。
- `research/compatibility-1.6.3/FIRST_PERSON_VALIDATION.md`：两平台逐项方法/布局审计、可选裁剪接口与角色覆盖边界。
- `research/compatibility-1.6.3/verify_first_person.py`：复用 BE 定向元数据提取器的只读审计脚本。
- `scripts/ValidateWindowsGame.py`：创建隔离模块索引并采集真实 Host 状态；`--elevate` 使用正常 UAC，`--attach` 可继续采集现有测试实例。

本轮 Windows 新示例包位于 `build/validation-163/examples/example.echo-windows.zip` 与 `example.first-person-windows.zip`。后续场景验证应先使用已有资料的角色检查切换、转向/FOV、切人及退出恢复，再检查新角色祀。

## 发布包检查

0.1.3 自包含目录自检通过：WPF 控件、WebView2 运行时、离线 Host 握手和宿主收尾均正常。0.1.3 native CTest 2/2 通过；上一 release 下载的原第一人称 ZIP 在 0.1.3 下通过实际 WebView2、配置、生命周期及代理 IPC 回归。Android APK 和第一人称 ZIP 原样沿用；SDK 文档更新，未改变 ABI。
