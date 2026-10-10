# 第一人称示例模块

模块 ID：`example.first-person`；版本：`0.1.0-dev`。使用 E Mod Loader 的原 BE format 1 / ABI 1 接口，提供 Windows x64 DLL、Android arm64 SO 和包内配置网页。**没有真实游戏或 Android 设备验证，当前是开发预览。**

## 使用

将构建产出的 ZIP 导入 E Mod Loader 并启用模块。模块默认允许第一人称功能，但不自动切换视角。

- Windows：游戏窗口获得焦点后按减号 `-` 切换；可在模块网页选择其他热键或关闭热键。
- 双端：网页可保存“默认进入第一人称”以及 FOV、头部隐藏、颈部填补范围、侧看角度与静止转身速度。
- 网页“切换/退出”按钮需要实际游戏宿主连接且游戏接口已就绪；离线模式显示“等待游戏运行时”，只能验证配置与通信。
- Android 暂无游戏内触控按钮；可在配置中设置默认开启，再进入游戏。独立管理应用对游戏中配置更新的设备行为未验证。

这里的“启用模块”是运行模块生命周期；“允许第一人称功能”是功能开关；“默认进入第一人称”是初始视角选择。热键与临时切换请求不会改写保存的初始视角设置。

## 保留与拆分

- 保留原 Cinemachine `PushStateToUnityCamera` 定位时机、头骨眼点偏移、游戏视角方向与 Dutch、FOV 和近裁剪调整。每次 push 后恢复传入的缓存状态。
- 保留当前角色 → ModelCom → ModelGo 的绑定、角色/模型/骨骼变化检查、静止状态下朝向调整及所有权检查。
- 保留独立头部的隐藏与阴影、混合网格的私有索引裁剪、颈口填补、角色骨骼资料，以及不可读几何时经过验证的骨骼回退；不确定的混合衣物保留。
- 移除自由相机、MMD、音乐、世界暂停、BE 覆盖层和 BE CustomModel 几何桥依赖。可读网格改用 Unity `Mesh.vertices/boneWeights/GetIndices`；不可读网格保留 GPU 读取路径。CPU 数组布局必须通过元数据验证。
- 历史来源版本为 BE 3.5.4 / 游戏 1.5.3；运行时按方法、字段和类型布局解析。其他游戏构建缺失必要接口时显示接口不匹配，不能由编译通过推断游戏兼容。

## 生命周期与线程

初始化在 Host worker 上启动等待器，延后查询可选运行时；解析元数据的线程显式附加/分离 IL2CPP。配置和消息只发布请求，Unity 状态操作在实际相机线程执行。

模块效果 hook 属于 `example.first-person`，由 Host 在停用时退休。一个标识为 `example.first-person.frame` 的进程常驻 frame/heartbeat 驱动负责收尾；即使游戏暂停或停用回调已返回，也能在下次游戏帧恢复模块仍拥有的状态。没有收尾工作时仅透传，不修改相机/模型、不调用网页桥，也不会在停用时清空共享链的稳定转发入口。

该驱动依赖 EML 提供的进程驻留模块库与游戏辅助接口。停用后若游戏不再执行任何相机帧/heartbeat，收尾会等待游戏恢复；遇到已经被游戏或其他模块接管的模型/朝向，不覆盖其状态。进程退出释放驻留代码，不进行热卸载。

## 构建与打包

```powershell
./scripts/BuildFirstPerson.ps1 -CMake <cmake.exe>
# 同时构建 Android：添加 -AndroidSdk <Android SDK 路径>
python scripts/PackageFirstPerson.py --android build/examples/first-person-android/package
```

默认输出：`releases/first-person/0.1.0-dev/example.first-person-0.1.0-dev.zip`。不传 `--android` 时生成仅 Windows 的包；ZIP 根目录直接包含 `module.json`。

实际 WebView2/ZIP/DLL 离线测试：

```powershell
dotnet run --project tests/EModLoader.DesktopTests -c Release -- <模块ZIP> <OfflineHost.exe> <输出目录> first-person
```

独立回归包括相机场景、头部/阴影所有权、可读 Unity 几何与 managed Mesh、CPU 裁剪、33 份角色资料，以及 V1 配置/消息/停用/再启用。测试是模拟场景与自有宿主的证据，游戏内效果和 Android 设备运行另行验证。

## 来源

来源：[Better Endfield](https://github.com/Dr-hydra/Better-Endfield) 的 `legacy/retired-before-next/first-person/`。

- 原归档来源提交：`ad33357bb1bb453d5925022e2be7bf507245d6ea`，产品版本 3.5.4，游戏版本 1.5.3。
- `archive/files/` 保留专用源码、角色骨骼资料、生成工具与测试。
- `archive/shared-snapshots/` 保留含第一人称的原相机及平台集成文件。
- `be-retirement-manifest.json` 为原归档清单；新迁入文件的逐项 SHA-256 见根目录 `provenance/migration.json`。

维护源码位于 `native/`，UI 位于 `ui/`，独立测试位于 `tests/`。几何与骨骼资料的原有命名空间和版权来源保留；拆分并修改的代码不声称与 BE 无关联。`archive/` 始终保留原始文件，不参与模块构建。项目采用 AGPL-3.0-only，JSON 依赖采用 MIT。
