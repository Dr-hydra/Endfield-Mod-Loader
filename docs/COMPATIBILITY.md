# 模块兼容性

Windows 0.1.3-dev已接入 BE 的正式官方父进程桥：自动发现官方 `Games.exe`，验证签名和发行组织后创建自己的暂停父进程，并保持到游戏退出。`launcher.json` 的 `official_parent` 可选指定路径，指定文件仍须通过验证。启动结果通过现有管道立即返回，保留按需 UAC 与 XInput 共存开关；启动失败清理本次创建的进程。该桥不改变模块 ABI，也不更新第三方模块自身的游戏地址或数据。

EML 0.1.0 支持原 Better Endfield **format 1 / ABI 1** 模块包。旧 ZIP、Windows DLL、Android SO 和网页脚本无需为加载器重新生成。Next 的原生导出与网页接口不在支持范围内。

## 接口

| 边界 | 支持约定 |
| --- | --- |
| 包清单 | ZIP 根目录的 `module.json`，`format: 1`、`abi: 1` |
| 原生入口 | `BetterEndfield_GetThirdPartyModuleV1` |
| SDK 头文件 | `include/BetterEndfield/` 中的 V1 结构、调用约定和返回值 |
| 网页 | `window.betterEndfield` 与 `better-endfield.module-ui.v1` 消息协议 |
| 业务配置 | 保留作者字段；升级保留原配置 |
| 生命周期 | Host worker 串行调用；已加载原生库保持进程驻留 |
| 可选能力 | `runtime`、`get_runtime`、`hooks`；使用前检查可用性 |

包中只包含另一平台原生库时，当前平台无法执行该库。UI-only 模块可以打开页面、保存配置，但没有原生消息接收端。

## 游戏与平台

包和 ABI 兼容只覆盖装载与通信。模块所用的游戏类型、方法、线程及对象生命周期需要由作者适配；原生模块能够加载，不代表游戏效果正确。

Windows 使用 x64 DLL；Android 使用 ARM64 SO。Android 动态依赖须能在目标游戏进程的 linker namespace 中解析，页面资源须包含在模块包中。

EML 采用独立安装与数据目录。Windows 0.1.2 可通过 [XInput 共存开关](XINPUT_COEXISTENCE.md) 保留其他工具的 `xinput1_4.dll` 并加载 EML。同一游戏逻辑被多个模块修改时，效果仍取决于模块自身的兼容性。

## 验证范围

已验证 Windows 原 BE Echo 二进制的装载、配置、消息、事件、停用和再次启用；实际 WebView2 验证了旧网页接口。Android 构建、网页协议及资源准备测试已通过。

2026-10-15 已在 Windows CN 1.6.3 验证真实官方父进程检查通过、EML Host 连通、Echo 消息往返及第一人称方法解析和 Hook 初始化（`ready`）。第一人称的模拟场景、网格和生命周期回归通过；Windows 1.6.3 与 Android 1.6.4 的当前方法及字段也完成定向静态审计。

第一人称场景内视点、输入、切人、头部隐藏及退出恢复仍待实测，Android 设备运行仍未验证。新角色祀尚无专用头饰/局部裁剪规则，不能用旧角色回归代表其效果。范围和证据见 [本轮验证记录](VALIDATION_1_6_3.md)。

模块开发与完整包约定见 [SDK](SDK.md)。
