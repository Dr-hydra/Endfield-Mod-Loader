# 第一人称示例：原样迁入阶段

来源：[Better Endfield](https://github.com/Dr-hydra/Better-Endfield) 的 `legacy/retired-before-next/first-person/`。

- 原归档来源提交：`ad33357bb1bb453d5925022e2be7bf507245d6ea`，产品版本 3.5.4，游戏版本 1.5.3。
- `archive/files/` 保留专用源码、角色骨骼资料、生成工具与测试。
- `archive/shared-snapshots/` 保留含第一人称的原相机及平台集成文件。
- `be-retirement-manifest.json` 为原归档清单；新迁入文件的逐项 SHA-256 见根目录 `provenance/migration.json`。

这里保存真实历史实现，不通过改名掩盖来源。当前公共快照仍依赖旧相机/MMD 等上下文，不是已经完成的独立可加载模块。待 E Mod Loader 加载方式和 SDK 确定后，再整理模块入口与必要依赖。
