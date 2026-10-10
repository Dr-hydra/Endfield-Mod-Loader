# 第一人称模块

E Mod Loader 的独立第一人称示例，模块 ID 为 `example.first-person`，版本为 `0.1.0-dev`。包含 Windows x64 DLL、Android ARM64 SO 和配置网页。

[下载模块](https://github.com/Dr-hydra/Endfield-Mod-Loader/releases/tag/v0.1.0-dev) · [加载器使用指南](../../docs/USER_GUIDE.md) · [模块 SDK](../../docs/SDK.md)

当前为开发预览，真实游戏和 Android 设备运行尚未验证。

## 安装与控制

1. 安装对应平台的 EML 加载器。
2. 在 EML 中直接导入 `example.first-person-0.1.0-dev.zip`，启用模块。
3. 打开模块页面设置参数，再启动游戏。

默认不切换视角。Windows 在游戏窗口获得焦点后按减号 `-` 切换，热键可修改或关闭；Android 可保存“默认进入第一人称”，当前没有游戏内触控按钮。页面的切换 / 退出按钮需要游戏宿主连接且游戏接口就绪。

启用模块与进入第一人称是两个开关。热键和临时切换请求不会改写保存的默认视角设置。

## 功能与参数

视点跟随当前角色头部，保留游戏视角输入。支持头部隐藏、阴影保留、颈口填补及静止时朝向调整；角色、模型或骨骼变化时重新建立绑定。

| 配置 | 默认值 | 说明 |
| --- | --- | --- |
| `enabled` | `true` | 允许第一人称功能 |
| `active` | `false` | 默认进入第一人称，仍可临时切换 |
| `hide_head` | `true` | 隐藏已确认会遮挡视线的头部 |
| `fill_neck` | `true` | 几何可读时尝试填补颈部开口 |
| `fov` | `75` | 视野，范围 30–120 度 |
| `neck_range` | `1` | 颈口填补范围，0.2–3 |
| `look_limit` | `90` | 侧看角度上限，0–180 度 |
| `turn_speed` | `360` | 静止转身速度，1–1440 度/秒 |
| `toggle_key` | `189` | Windows 虚拟键码；0 关闭热键 |
| `diagnostics` | `false` | 详细诊断日志 |

## 运行限制

- 必要的游戏方法或字段布局不匹配时，模块报告接口不匹配或加载失败。
- 离线宿主用于配置和通信测试，不提供第一人称游戏效果。
- 头部隐藏及颈口填补依赖角色骨骼、网格与可用的读取接口；不能确认的混合衣物保留。
- 只有确认角色处于静止状态时才调整朝向，游戏接管的朝向和模型状态不会被覆盖。
- 停用在游戏线程恢复状态；游戏暂停且相机帧/heartbeat 不执行时，收尾等待游戏恢复。
- 不依赖 BE 的自由相机、MMD、音乐、覆盖层或模型几何桥；不保证与其他相机模块同时使用的效果。

## 构建与验证

```powershell
./scripts/BuildFirstPerson.ps1 -CMake <cmake.exe>
# 添加 -AndroidSdk <SDK路径> 构建 Android SO
python scripts/PackageFirstPerson.py --android build/examples/first-person-android/package
```

不传 `--android` 时生成仅 Windows 的 ZIP。默认输出位于 `releases/first-person/0.1.0-dev/`。

维护源码在 `native/`，网页在 `ui/`，测试在 `tests/`。构建脚本执行 6 组 native 回归，覆盖相机、角色切换、几何、阴影、配置和收尾。实际 ZIP / DLL / WebView2 的离线测试命令见 [构建说明](../../docs/DEVELOPMENT.md)。这些测试不代表游戏实机通过。

V1 入口与配置回调运行在 Host worker，游戏状态操作由实际相机线程处理。模块库与用于收尾的 frame 驱动保持进程驻留，停止效果后驱动仅处理仍拥有的恢复工作，完成后透传。

## 许可与来源

采用 AGPL-3.0-only。相机、网格算法与角色资料源自 Better Endfield 第一人称归档，源提交为 `ad33357bb1bb453d5925022e2be7bf507245d6ea`。`archive/` 保存原始来源，不参与模块构建。依赖许可见 [第三方声明](../../THIRD_PARTY_NOTICES.md)。
