# 模块兼容性

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

EML 采用独立安装与数据目录。不提供与 BE 或其他加载器同时作用于同一游戏进程的兼容保证。

## 验证范围

已验证 Windows 原 BE Echo 二进制的装载、配置、消息、事件、停用和再次启用；实际 WebView2 验证了旧网页接口。Android 构建、网页协议及资源准备测试已通过。

第一人称通过模拟场景、网格和生命周期回归。真实游戏及 Android 设备运行尚未验证，不能将离线测试结果视为游戏实机通过。

模块开发与完整包约定见 [SDK](SDK.md)。
