# E Mod Loader 0.1.0 开发预览

日期：2026-10-10。在独立 `F:/Endfield Mod Loader` 工作区实现，原迁入文件保持不变。没有执行真实游戏加载，也没有手机安装或运行。

## 已实现

- 独立 WPF 管理应用：用户 Logo、模块库、ZIP 导入、启用/停用、配置、排序、移除、日志、网页容器、工坊/仓库入口。
- 原 BE format 1 / ABI 1 兼容：原导出与结构布局、`window.betterEndfield`、原消息信封和作者业务配置保留；不要求兼容 Next。
- 独立 native Host：依赖排序、不可变代次、模块生命周期、配置/消息/事件/日志、凭据验证与共享 Hook 链；停用不热卸载 DLL/SO。
- Windows 显式启动组件和游戏 Host；保留运行时就绪检查、GC 等待与可选辅助接口后续提供。管理应用和离线宿主不会被标成真实游戏连接。
- Android 独立 APK 工程、Logo、本地模块管理、旧网页容器、`:offline` 宿主服务、libxposed API 102 目标入口、资源发布/私有代次、Unity 首帧加载和 JNI 注册。
- 双端引擎辅助接口和 Hook backend 构建接入；Android Dobby 写入器覆盖跨页写入与失败传播，来源记录明确保留。
- 构建脚本、兼容 SDK、Echo 作者示例、产品回归与来源许可记录。

## 验证证据

| 验证 | 结果与范围 |
| --- | --- |
| Windows .NET/WPF Release | 编译通过；自包含分发包生成 |
| Windows native Release | OfflineHost、Host DLL、Launcher 和测试目标构建通过 |
| 产品模块管理与真实旧 DLL | 19 项检查通过：原包/二进制、路径拒绝、配置、消息/事件、启停、升级代次与文件保留 |
| 实际 WebView2 | 原发行 HTML/JS 打开成功；旧 API 的 read/save/status/send/reply/event 通过；桌面与模块页面截图已检查 |
| Hook 链 | 真实测试函数的传参、返回、调用顺序、一次原函数调用、旧接口重复拒绝和 owner retirement 通过 |
| Windows 启动接入 | 仅自有受控进程：标准加载产品 Host 与身份验证后的状态握手通过；不代表游戏实机通过 |
| 原发行 DLL 集合 | 产品 OfflineHost 加载 3.5.1/3.5.2/3.5.3 原 Echo DLL，以及 3.5.4 本地附件；配置/消息/生命周期回归通过，没有修改或重新编译这些 DLL |
| Android materializer | 实际产品 Java 代码与 JVM 适配器通过 16 项路径、代次、配置和失败隔离检查 |
| 双端网页协议 | 原 creator API、模块身份边界、回复/事件、取消订阅、错误与超时测试通过 |
| Android Debug APK/原生库 | Java/NDK 构建通过；Lint 0 errors、9 warnings；APK v2 签名验证通过；arm64 Host 的 LOAD 段按 16 KiB 对齐 |

构建、APK Lint、签名和包布局的最终结果随本轮产物记录，不把未执行的设备检查填写为通过。

## 限制与下一步

- 第一人称归档尚未提取为可加载模块，本轮分发包不包含可用第一人称插件。
- 实际游戏启动权限、运行时线程、游戏辅助能力与模块游戏效果没有实机验证。旧包兼容证据覆盖发布 Echo 的已执行路径，不代表全部第三方业务代码均已验证。
- Android 框架版本/作用域、真实 linker namespace、nativeRender 时序、WebView 与旧 SO 执行没有手机验证；开发 APK 不作为正式签名发行版。
- Windows 模块网页需要 WebView2 Runtime；游戏要求管理员权限时需要使用相应权限运行软件。现有 XInput 加载方式冲突会被报告。
- 不实现 BE/EML 跨产品互斥；无反作弊识别或账号结果保证。
