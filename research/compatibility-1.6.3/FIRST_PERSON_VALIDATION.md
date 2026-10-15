# EML 示例在 Windows 1.6.3 / Android 1.6.4 的验证记录

2026-10-15。EML 工作区为 `G:\Endfield Mod Loader`，游戏为 `G:\Arknights Endfield`；Windows 客户端 1.6.3 / Unity 2021.3.34f5 的版本证据沿用 BE 当日针对该安装的检查。未计算产物哈希。

## 结论与边界

Echo 和第一人称均可重新构建，第一人称的必要方法和字段与当前 Windows 游戏匹配。真实游戏宿主已经完成第一人称初始化并报告 `ready`，Echo 已能调用运行时辅助接口并完成消息往返。

尚未进入可操控场景验证视点、输入、切角、头部隐藏、颈口填补、阴影和停用恢复。因此，此记录只确认构建、离线回归、接口匹配及 Windows 真实游戏初始化，不能作为第一人称场景效果验收。Android 本轮重新编译两个 ARM64 示例，并以本地 1.6.4 样本完成接口与布局静态审计；未运行设备上的 EML。

## 新构建与回归

全部构建使用独立目录 `build/validation-163/examples/`，没有覆盖发布包和既有构建。CMake 配置和实际编译命令可由各构建目录的 `CMakeCache.txt` / `.vcxproj` / `build.ninja` 复核；[六组回归日志](../../build/validation-163/examples/first-person-windows/Testing/Temporary/LastTest.log)保留了具体执行结果。

| 项目 | 结果 |
| --- | --- |
| Echo Windows x64 | Release 编译通过 |
| 第一人称 Windows x64 | Release 编译通过 |
| 第一人称 native 回归 | 6/6 通过 |
| Echo Android ARM64 | NDK 27.2.12479018 / API 29 编译通过 |
| 第一人称 Android ARM64 | 同上，编译通过 |

六组回归覆盖相机与角色生命周期、网格归属及阴影恢复、托管 Mesh 回退、几何/颈口/蒙皮输入、角色规则、V1 配置与离线生命周期。其中有计数的五组分别执行 1,232、521、50、2,163、38 项检查，另有一组几何回归。

新 Windows 测试包为 [Echo ZIP](../../build/validation-163/examples/example.echo-windows.zip) 和[第一人称 ZIP](../../build/validation-163/examples/example.first-person-windows.zip)，由本轮新 DLL、清单及 UI 生成；用于宿主代理的实际 ZIP / DLL / WebView2 回归，不替换发布附件。

## 当前 Windows 元数据审计

可重跑命令：

```powershell
python research/compatibility-1.6.3/verify_first_person.py `
  --be-root 'G:\Better Endfield' `
  --game 'G:\Arknights Endfield' `
  --output build/validation-163/examples/first-person-contract-audit.json
```

脚本使用 BE 现有只读 PE/IL2CPP 提取器，只提取第一人称引用的类和几何结构；本次为 37 类、3,839 个类内方法。没有生成全量 dump 或 IDA 数据库。原始游戏文件只记录大小及修改时间。[逐项审计 JSON](first-person-contract-audit.json)随本记录保存。

| 检查 | 结果 |
| --- | --- |
| 源码方法描述符 | 80 个签名匹配且入口非零；2 个可选方法被裁剪 |
| 源码字段描述符 | 14/14 匹配 |
| Vector3 / BoneWeight / AsyncGPUReadbackRequest / SubMeshDescriptor 字段布局 | 20/20 匹配 |
| 核心相机 Hook | 4/4 匹配，BE 当日内联审计均为 `ok` |

两个缺失方法是 `Mesh.GetSubMesh` 和 `Mesh.GetIndexStart`。它们均为可选项，现有 `FpReadManagedSubmesh` 使用 `GetIndices(submesh, true)` 及索引长度回退；`portable_mesh` 回归覆盖该分支。该情况也出现在 Windows，源码原有“Android 裁剪”注释不能解释为只有 Android 会触发回退。

四个关键入口的当前 RVA 为 `_ProcessDitherByPitch=0x56159D0`、`CameraManager.TailLateTick=0x611BA50`、`PushStateToUnityCamera=0x5608830`、`Time.get_unscaledDeltaTime=0x5150EF0`。这些值仅为审计证据，示例仍按 IL2CPP 名称与签名动态解析，不使用固定 RVA。

BE 复用证据位于 `G:\Better Endfield\research\host\1.6.3\Windows\hook-update\hook-inline-scan.json`。静态的 `ok` 不替代场景中确认 Hook 实际触发。

## 真实游戏初始化证据

[真实宿主日志](../../build/validation-163/game-baseline/modules/host.log)已记录：

```text
runtime: Optional engine helpers ready
example.first-person: Optional/missing method: fp.mesh.submesh
example.first-person: Optional/missing method: fp.mesh.index_start
example.first-person: First person: CameraState layout resolved (header=0x10 rawPos=0xA4 rawRot=0xB0 lens=0x44 fov=0x0 nearClip=0x8 dutch=0x10)
example.first-person: First-person module: ready
```

真实缺项与静态审计一致。Echo 的真实宿主回复还确认 `runtime_helper_ready=true`；完整启动链路及官方父进程验证由本轮启动检查单独记录。

## Android 1.6.4 静态审计

输入来自 BE `inputs/1.6.4/Android/`，原始 `source.json` 标记客户端 `1.6.4 / versionCode 61 / arm64-v8a`，采集日期 2026-10-15。本任务没有连接手机，未改写设备或原始输入；本地临时输入通过硬链接引用已有的可读 metadata 副本和 `libil2cpp.so`。

复用 `G:\Better Endfield\research\android\1.6.4\Android\hook-update\extract_android_targets.py`，使用 `--metadata-only`，类列表保存于 [android-target-classes.txt](android-target-classes.txt)。仅提取 38 类、3,852 个类内方法，使用该工具现有 ELF 相对重定位、模块名和字段偏移掩码处理。

检查结果与 Windows 一致：**80 个方法签名及非零入口通过、2 个可选 Mesh 方法缺失、14/14 字段通过、20/20 原生布局字段通过**。没有发现当前示例需要新增的 Android 版本差异分支。[Android 逐项审计 JSON](first-person-android-164-contract-audit.json)随本记录保存。

已有目标元数据可直接复核：

```powershell
python research/compatibility-1.6.3/verify_first_person.py `
  --be-root 'G:\Better Endfield' `
  --game 'G:\Better Endfield\inputs\1.6.4\Android' `
  --platform android `
  --metadata build/validation-163/examples/android-targets/targeted-contracts.json `
  --output build/validation-163/examples/first-person-android-164-contract-audit.json
```

Android 检查没有执行 EML 的 `il2cpp_resolve_icall`、真实 Hook 安装、GPU readback 或第一人称场景效果；BE 自身在手机上的 Camera 初始化证据不等同于 EML 第一人称通过。

## 角色资料覆盖

`first_person_profiles.generated.inc` 仍包含原有 33 个角色的专用骨骼/局部裁剪规则，未纳入 BE 当前目录已有的 `chr_0036_jsspsi`（祀）。这不使通用头骨跟随和头部判断失效，但新角色的特殊头饰、混合网格和颈口效果需要实机或当前网格证据；旧角色规则回归不能代表新角色效果已通过。本轮没有在缺少此类证据时扩充生成资料。

下一次进入可操控场景时，应先用一个已有资料的角色核对减号切换、转向/FOV、切人、阴影及退出恢复，再用新角色核对头饰和混合网格。离线回归和真实 `ready` 均不覆盖这些视觉结果。
