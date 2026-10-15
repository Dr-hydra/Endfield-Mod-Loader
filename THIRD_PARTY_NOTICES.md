# 来源与第三方许可

- E Mod Loader 使用 AGPL-3.0-only。模块接口、部分宿主与资源准备代码，以及第一人称实现，派生自 [Dr-hydra/Better-Endfield](https://github.com/Dr-hydra/Better-Endfield)。原始来源清单见 `provenance/migration.json`。
- 原 BE SDK 1.0.0 的公开 C ABI 作为兼容边界保留于 `include/BetterEndfield/`。
- Windows 官方父进程桥派生自 BE 本机 `native/loaders/shared/official_launcher.{h,cpp}` 的 2026-10-15 实现；EML 的官方启动器发现、签名验证及父进程管理位于 `src/native/official_parent.{h,cpp}`，按原项目 AGPL-3.0-only 维护。
- `examples/first-person/native/` 的相机定位、网格算法与角色资料派生自 BE 已归档的第一人称实现（源提交 `ad33357bb1bb453d5925022e2be7bf507245d6ea`）；维护副本与原始归档分开，使用 AGPL-3.0-only。独立入口、配置网页、线程收尾与 Unity 可读网格适配在 EML 中维护。
- MinHook：BSD 2-Clause，源码和许可证位于 `src/native/vendor/minhook/`。
- nlohmann/json：MIT，许可证保留在 `native/shared/third_party/nlohmann/json.hpp` 文件头。
- Windows 界面使用 .NET/WPF 与 Microsoft WebView2，遵循各自 SDK 和运行时许可。WebView2 Runtime 由系统安装的 Microsoft Edge WebView2 Runtime 提供。
- Android 使用 libxposed API/service 102 和 Dobby，按照各项目许可证使用。Dobby 从构建参数指定的外部源码目录静态构建；不分发构建机工具链。
- 应用 Logo 由项目维护者提供。

完整项目许可证见 [LICENSE](LICENSE)。
