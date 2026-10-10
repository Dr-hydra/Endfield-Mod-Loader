# 已发行模块包兼容计划

硬要求：原 Better Endfield 已发行模块 ZIP 可以直接导入使用，不修改原生库或网页，不要求作者重新编译。按照最新范围，Next 不需要兼容。

这项要求优先于 EML 新命名。保留兼容契约会保留相应旧字符串和接口关联；不应把它表述为代码或识别上的完全无关联。

## 核对依据

检查本机 `Better Endfield/releases/3.5.1`、`3.5.2`、`3.5.3` 的实际 SDK 与双端 Echo 发行附件，以及 `3.5.4` 的本地附件，确认（3.5.4 附件存在不等于该版本已正式发布）：

- 包使用 format 1、ABI 1。
- Windows DLL 和 Android SO 的模块导出为 `BetterEndfield_GetThirdPartyModuleV1`。
- Echo 网页使用 `window.betterEndfield`。
- 原 BE 公共头文件与迁入的 Next 头文件在归一化命名前缀后保持相同结构/定义；实际 ABI 仍由编译及运行验证。

## 必须保留的边界

| 边界 | 兼容要求 |
| --- | --- |
| ZIP 和 manifest | 接受原 `module.json` 的格式和字段；不强制 `loader: "eml"`；保留模块 ID、依赖、原始配置和包内路径 |
| 导出 | 查询原 `BetterEndfield_GetThirdPartyModuleV1`，不要求 DLL/SO 增加新导出 |
| 二进制 ABI | 保留大小、对齐、字段顺序、调用约定、结果值；内部对象不能直接当作旧结构传递 |
| 生命周期 | 保留回调线程约定、字符串借用、停用收尾、再次启用和驻留/更新行为 |
| 基础 Host | 模块身份、平台、包根目录、日志、reply/emit、配置/消息按原约定传递 |
| 可选 runtime/Hook | 旧结构和调用语义由适配服务提供；未就绪或不支持应报告，不能伪造成功 |
| 网页 | 注入原 `window.betterEndfield` 方法、回复/事件格式；原 `better-endfield.module-ui.v1` 协议和 Android JS transport 名称由兼容入口处理 |
| 权限和归属 | 网页仍绑定自己的模块身份；token 不交给网页；采用独立 EML 安装/配置/浏览器目录 |

EML 私有类名、源码布局、产品文件名、Android applicationId、后端传输实现和状态管理可以独立重构。作者自己的配置字段不是加载器字段，不批量改名。

## 研究原型

[原型工程](../research/compatibility/CMakeLists.txt) 在 build 目录生成原 Host 的研究副本，只把模块入口查询改回已发行 BE 导出。原样迁入文件不改动。此工程不代表正式 EML 产品已经实现兼容层。

[发行包验证脚本](../research/compatibility/check_published_packages.py) 从指定的实际 release 目录提取原 Echo DLL，直接交给研究 Host 执行原生命周期/通信回归；检查 DLL 与 ZIP 未被修改，记录 ZIP、Windows DLL 和 Android SO 的 SHA-256。

```powershell
cmake -S research/compatibility -B build/research/compatibility -A x64
cmake --build build/research/compatibility --config Release
python research/compatibility/check_published_packages.py --releases-root "F:/Better Endfield/releases" --host build/research/compatibility/Release/EMLCompatibilityHost.exe --output build/research/compatibility/published-tests
```

当前验证范围是 Windows 原生基础装载、配置、消息、事件、启停与代次更新；Android SO 的符号及网页 API 已静态核对，但 Android 和网页运行需要后续正式适配后验证。当前没有实机游戏验证条件，按 [离线验收计划](OFFLINE_VALIDATION.md) 推进；第一人称及依赖具体游戏辅助能力的模块的游戏内效果标记未验证，不以 Echo 通过替代，也不因此阻塞产品开发。

2026-10-10 验证结果：研究 Host 已仅查询原 BE 导出；3.5.1、3.5.2、3.5.3 发行附件及 3.5.4 本地附件中的原 Echo Windows DLL 均加载成功并通过生命周期/消息回归，没有重编译或修改这些 DLL/ZIP。实际 SDK 的三份公开头文件在去除注释和归一化产品前缀后与迁入定义一致。[校验基线](../research/compatibility/verified_baselines.json) 保存附件哈希与验证范围，完整 EML 产品兼容性仍待实施。

## 后续实施

1. 将旧公开 ABI 固定为兼容边界，建立正式 EML Host 和相应适配对象。
2. 在管理界面和网页容器接入旧包/旧桥，原已发行 ZIP 作为端到端测试输入。
3. 双端运行旧 native/UI 示例，覆盖配置保存、回复、事件、日志、停用、升级与重启。
4. 对依赖 runtime、Hook 或特定游戏接口的模块分别记录实际验证范围。
5. 新接口另行协商版本；已发行 V1 的可用性不依赖作者升级。
