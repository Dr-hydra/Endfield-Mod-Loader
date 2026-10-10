# E Mod Loader 兼容 SDK

首版继续接受原 Better Endfield format 1 / ABI 1 模块包。已发行 ZIP、原生二进制和网页脚本无需重新生成；Next 不在兼容目标内。

新模块也可使用 `include/BetterEndfield/` 的三个头文件和 `examples/echo/` 示例，导出 `BetterEndfield_GetThirdPartyModuleV1`，网页调用 `window.betterEndfield`。

Host 回调在 worker 串行执行；借用的 UTF-8 字符串必须在需要时复制。Hook 回调运行于目标的实际线程。可选 `runtime` 和 `hooks` 需要检查是否可用；游戏辅助能力就绪不等于任意 Unity 方法可以从 worker 调用。Windows/Android 的调用约定、结构体布局和返回码保留原 V1。

模块升级使用不可变代次，已加载二进制改变后需要重启宿主或游戏。停用调用生命周期收尾并停用本模块链节点，原生库和转发入口保持进程驻留。

管理应用以本模块身份绑定网页，token 不交给网页。仅包内资源可作为页面资源加载。作者自带资源、线程和游戏状态的生命周期由模块负责。

完整包、配置和生命周期背景可参阅原 [模块创作者指南](host/THIRD_PARTY_MODULE_CREATOR_GUIDE.md)；其中 Next 品牌的导出和网页名称不作为本产品接口，本文与 `include/BetterEndfield/` 为当前维护契约。

构建示例：

```powershell
cmake -S examples/echo -B build/examples/echo -A x64
cmake --build build/examples/echo --config Release
```

输出 package 目录可直接压缩为 ZIP。双端包合并对应 `native/windows-x64` 和 `native/android-arm64` 内容并保留两平台 manifest。兼容验证范围见 [兼容计划](COMPATIBILITY_PLAN.md)，游戏效果未验证。
