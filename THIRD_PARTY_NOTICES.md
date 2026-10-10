# 来源与第三方许可

- E Mod Loader 使用 AGPL-3.0-only。第三方模块 Host、兼容界面与资源准备，以及第一人称归档，派生自 [Dr-hydra/Better-Endfield](https://github.com/Dr-hydra/Better-Endfield)。原样迁入文件见 `provenance/migration.json`，产品初始化来源见 `resources/source-initialization.json`。
- 原 BE SDK 1.0.0 的公开 C ABI 作为兼容边界保留于 `include/BetterEndfield/`。
- `examples/first-person/native/` 的相机定位、网格算法与角色资料派生自 BE 已归档的第一人称实现（源提交 `ad33357bb1bb453d5925022e2be7bf507245d6ea`）；维护副本与原始归档分开，使用 AGPL-3.0-only。独立入口、配置网页、线程收尾与 Unity 可读网格适配在 EML 中维护。
- MinHook：BSD 2-Clause，源码和许可证位于 `src/native/vendor/minhook/`。
- nlohmann/json：MIT，许可证保留在 `native/shared/third_party/nlohmann/json.hpp` 文件头。
- Windows 界面使用 .NET/WPF 与 Microsoft WebView2，遵循各自 SDK 和运行时许可。WebView2 Runtime 由系统安装的 Microsoft Edge WebView2 Runtime 提供。
- Android 使用 libxposed API/service 102 和 Dobby，按照各项目许可证使用。Dobby 从构建参数指定的外部源码目录静态构建；不分发构建机工具链。
- Logo 由用户提供，产品直接使用原 PNG；ICO 仅封装同一 PNG 数据，没有重绘或更改像素。

兼容接口和来源记录明确保留旧名称，不宣称本项目与原实现无关联。
