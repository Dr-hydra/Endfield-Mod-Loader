# 第一人称模块实施记录

日期：2026-10-10。模块 `example.first-person`，版本 `0.1.0-dev`。

## 结果

从 BE 归档相机模块提取第一人称，生成 EML V1 模块入口、Windows x64 DLL、Android arm64 SO、包内配置网页和双端 ZIP。保留原 CameraState push 时机、姿态还原、角色/场景/骨骼变化检查、头部与阴影所有权、颈口算法和角色资料。

自由相机、MMD、音乐、世界暂停、BE 覆盖层与 CustomModel 几何桥不参与构建。可读网格使用 Unity 公共数组，验证 Vector3/BoneWeight 的类型大小、字段偏移、数组长度、权重与索引；不可读网格保留 GPU 读取和骨骼回退。不修改原 V1 SDK、产品 Host、BE 或原归档。

模块在等待游戏辅助能力时仍能提供配置和状态。元数据线程显式附加 IL2CPP，实际游戏状态操作交给相机线程。模块停用立即停止效果请求；驻留 frame/heartbeat 驱动只在游戏线程收尾，常态透传，保留共享链的稳定转发入口。无法确认对象存活时保留恢复所有权，等待后续确认。

## 验证

| 检查 | 结果与范围 |
| --- | --- |
| Windows Release DLL 与 6 组 native 回归 | 通过；相机姿态/角色切换 1232 次断言，头部/阴影/恢复 521 次断言，可读 Unity 几何/managed Mesh 50 次断言，CPU 裁剪/重试测试、角色资料 2163 次断言、V1 配置/消息/接口解析/停用/驻留驱动 38 次断言 |
| .NET WebView2 测试工程 | Release 编译通过，0 warnings / 0 errors |
| 实际 EML ZIP/DLL/WebView2 | 导入并加载新模块；页面保存 FOV，native 返回 waiting_runtime；离线切换被拒绝；Host 停用及再次初始化同一驻留 DLL 通过；页面截图已检查 |
| Android arm64 Release SO | 编译与链接通过；不需要 BE 原生库或动态 C++ runtime；保留 V1 导出 |
| Android ELF | LOAD 段按 16 KiB 对齐；依赖为系统 libc/libm/libdl |
| 原样迁入来源 | 103 文件 SHA-256 全部通过；`archive/`、原 SDK 和来源快照没有修改 |

模拟回归直接执行维护代码，但不具备真实游戏对象、GPU、Android linker namespace 或反作弊环境。没有启动游戏，没有 Android 安装/设备执行；游戏效果与设备行为均为 **not_run**。

## 交付与限制

使用 `scripts/BuildFirstPerson.ps1` 构建，`scripts/PackageFirstPerson.py` 打包。双端 ZIP 的当前文件大小和 SHA-256 由同名 `.json` 记录。该包作为独立模块导入现有 EML 开发预览，不重新发布初始加载器分发包，不自动安装到用户模块库。

- 游戏接口缺失、字段布局不符或 hook 无法安装时显示失败状态，保留日志。
- 默认不开启视角；Windows 减号切换，Android 可配置默认进入。暂无游戏内 Android 触控按钮。
- 暂停期间若相机帧和 heartbeat 都不执行，停用恢复等待游戏继续；不从 worker 强行调用 Unity。
- 公共 Mesh getter 被裁剪、网格不可读且 GPU 读取不可用时，保留经过确认的独立部件/骨骼回退；不能确认的混合衣物和颈口保留。
- 不提供 BE/EML 跨产品互斥或同时使用保证。

来源、构建和使用说明见 [模块 README](../examples/first-person/README.md)。
