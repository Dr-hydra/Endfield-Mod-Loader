# E Mod Loader 模块 SDK

适用：模块包 **format 1 / Native ABI 1**。Windows x64 和 Android ARM64 使用同一套清单、配置及网页协议。

从 [Release](https://github.com/Dr-hydra/Endfield-Mod-Loader/releases/tag/v0.1.0-dev) 下载 `EModLoader-SDK-0.1.0.zip`，或使用仓库的 `include/BetterEndfield/` 和 `examples/echo/`。原 BE V1 包无需重新编译；兼容范围见 [兼容性说明](COMPATIBILITY.md)。

## 模块包

ZIP 根目录直接包含 `module.json`，不增加外层目录：

```text
module.json
native/windows-x64/example.echo.dll
native/android-arm64/libexample.echo.so
ui/index.html
ui/style.css
ui/app.js
```

```json
{
  "format": 1,
  "id": "example.echo",
  "name": "Echo / Counter",
  "author": "Your name",
  "version": "1.0.0",
  "abi": 1,
  "libraries": {
    "windows-x64": "native/windows-x64/example.echo.dll",
    "android-arm64": "native/android-arm64/libexample.echo.so"
  },
  "ui": "ui/index.html",
  "default_configuration": { "label": "Echo" },
  "dependencies": []
}
```

ID 为稳定 ASCII 标识，格式为 `[A-Za-z0-9][A-Za-z0-9_.-]{0,95}`，不得使用产品保留名称。原生入口 ID 必须与 manifest 完全相同。`name`、`author`、`version` 为字符串。

`libraries` 可省略不支持的平台，`ui` 可省略；原生库和网页至少有一种。路径使用包内相对路径和 `/`，不得包含绝对路径、`..`、反斜线或冒号；不依赖软链接或包外网页资源。

`dependencies` 为其他模块 ID，不能依赖自身，最多 128 项。缺失、停用、失败或循环依赖会阻止启动；EML 不自动下载依赖。

配置必须是 JSON 对象。首次安装使用 `default_configuration`，同 ID 更新保留启用状态及原配置；新增配置字段的默认值和迁移由作者处理。已加载二进制更新后需要重启宿主或游戏。

## 原生入口

公开头文件：`ThirdPartyModule.h`、`ModuleApi.h`、`HookChain.h`。导出固定入口 `BetterEndfield_GetThirdPartyModuleV1`：

```cpp
#include <BetterEndfield/ThirdPartyModule.h>

static BE_Result BE_CALL Initialize(const BE_ThirdPartyHostV1*, const char*);
static BE_Result BE_CALL Configure(const char*);
static BE_Result BE_CALL Message(const char*, const char*);
static void BE_CALL Shutdown();

static const BE_ThirdPartyModuleV1 module = {
    sizeof(BE_ThirdPartyModuleV1), 1, "example.echo",
    Initialize, Configure, Message, Shutdown
};

#if defined(_WIN32)
#define MODULE_EXPORT __declspec(dllexport)
#else
#define MODULE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" MODULE_EXPORT const BE_ThirdPartyModuleV1* BE_CALL
BetterEndfield_GetThirdPartyModuleV1() { return &module; }
```

`initialize` 与 `on_message` 为必需回调；配置变化和关闭回调可为空。结构大小、版本、字段顺序与调用约定以 SDK 头文件为准。

| Host 字段 | 用途 |
| --- | --- |
| `module_id`、`platform`、`package_root` | 本模块身份、当前平台和安装代次目录 |
| `log(context, message)` | 本模块诊断日志 |
| `reply(context, request_id, result, json)` | 回复网页请求 |
| `emit(context, json)` | 推送模块事件 |
| `runtime`、`get_runtime(context)` | 可选游戏辅助能力，可延后就绪 |
| `hooks` | 可选共享 Hook 链 |

生命周期回调在 Host worker 上串行执行，**不是 Unity / 游戏主线程**。Hook 回调运行在实际调用目标的线程。需要游戏线程的操作由模块安排到正确的调用时机；runtime 就绪不改变线程要求。

回调中的 UTF-8 字符串仅临时借用，需要在返回后使用时必须复制。模块负责自己的线程、对象、任务和状态恢复。停用后可能再次初始化；DLL / SO 保持进程驻留，不执行热卸载。不要在停用后继续使用生命周期上下文。

返回值：`0 Ok`、`1 InvalidArgument`、`2 NotReady`、`3 NotFound`、`4 ContractMismatch`、`5 Conflict`、`6 Failed`。

## 网页与消息

网页使用包内静态 HTML、CSS、JavaScript 及资源。Windows 使用 WebView2，Android 使用 WebView；外部网页和 CDN 资源不作为模块页面加载。

双端兼容 API 为 `window.betterEndfield`：

```javascript
const host = window.betterEndfield;
const configuration = await host.readConfig();
await host.saveConfig({ ...configuration, label: "My module" });

const unsubscribe = host.onmessage(message => {
  if (message.kind === "reply") {
    console.log(message.request_id, message.result, message.body);
  } else if (message.kind === "event") {
    console.log(message.body);
  }
});

await host.send({ action: "count" });
const status = await host.status();
// status: { connected, mode, module }
```

| 方法 | 行为 |
| --- | --- |
| `readConfig()` | 读取自身 JSON 配置 |
| `saveConfig(object)` | 持久化配置，连接时通知 Host；返回 `{saved:true}` |
| `send(body)` | 投递消息；返回值表示接收情况，不表示业务完成 |
| `status()` | 连接模式与本模块运行状态 |
| `onmessage(listener)` | 订阅回复和事件，返回取消订阅函数 |

原生业务回复通过 `onmessage` 接收：

```json
{"kind":"reply","request_id":"...","result":0,"body":{"counter":1}}
```

网页绑定本模块身份，不能通过传入其他 `module_id` 操作别的模块。应用注入兼容协议 `better-endfield.module-ui.v1`；作者无需接触宿主 token 或自行连接 HTTP 服务。

离线时可以保存配置，下次加载使用持久化配置。UI-only 模块没有原生接收端。页面关闭不等于停用模块；消息和事件不持久化，请求超时及业务重试由页面处理。

## 游戏辅助与 Hook 链

使用 runtime 前检查指针及所需函数是否可用。辅助接口提供方法/字段/类型解析、托管调用、字符串、GC handle 与 Hook 注册；模块仍需适配游戏版本、类型和方法签名。

使用 `host->hooks` 前检查版本、结构大小和函数指针。`create(context, module_id, target, detour, &next, &handle)` 注册节点，`disable` 停用节点，`disable_module` 停用所属模块节点。

同一目标的节点按实际注册顺序调用，`next` 是稳定的后继转发入口，最终到原函数。detour 必须符合目标的平台 ABI；通常调用一次 `next`，直接重新调用目标入口可能递归。停用后转发入口仍驻留。

`runtime->create_hook` 与共享接口使用同一条链，但重复为同一模块注册已活动的同一目标会返回 `Conflict`。重新启用时重新注册并保存新的输出，不清空仍可能被回调使用的稳定转发入口。

共享链只协调参与其中的 Hook，不自动解决游戏状态冲突；模块自行安装的其他 Hook 不由 Host 管理。

## 编译与示例

Echo：

```powershell
cmake -S examples/echo -B build/echo-windows -A x64
cmake --build build/echo-windows --config Release
```

输出 `package/` 目录可直接压缩，ZIP 根目录保持 `module.json`。双端包合并两个平台的库，并保留相应 `libraries` 字段。

Windows 需要 Visual Studio C++ 工具；Android 使用 NDK 和 arm64，动态依赖需在目标游戏进程中解析。示例静态链接 C++ runtime，并为 Android 配置 16 KiB 页面支持。

[第一人称示例](../examples/first-person/README.md) 展示延后查询 runtime、游戏线程消费请求、相机缓存恢复及停用后的收尾。构建命令与离线回归见 [构建说明](DEVELOPMENT.md)。

发布模块时说明已测试的平台与游戏版本。包格式校验和离线宿主不能代替游戏业务、线程和状态恢复验证。
