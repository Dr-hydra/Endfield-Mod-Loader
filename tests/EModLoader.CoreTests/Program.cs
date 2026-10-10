using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using EModLoader.Core;

if (args.Length != 2) throw new ArgumentException("Supply original Echo ZIP and product OfflineHost.exe.");
string fixture = Path.GetFullPath(args[0]), executable = Path.GetFullPath(args[1]);
string root = Path.Combine(Path.GetTempPath(), "eml-tests-" + Guid.NewGuid()); Directory.CreateDirectory(root);
int checks = 0;
void Check(bool condition, string message) { checks++; if (!condition) throw new Exception(message); }
async Task Reject(Func<Task> action, string message) { try { await action(); } catch { checks++; return; } throw new Exception(message); }
string Zip(string id, string? bad = null)
{
    string path = Path.Combine(root, Guid.NewGuid() + ".zip");
    using var archive = ZipFile.Open(path, ZipArchiveMode.Create);
    var manifest = new JsonObject { ["format"] = 1, ["abi"] = 1, ["id"] = id, ["name"] = "UI test", ["author"] = "test", ["version"] = "1", ["libraries"] = new JsonObject(), ["ui"] = "ui/index.html" };
    foreach (var (name, text) in new[] { ("module.json", manifest.ToJsonString()), ("ui/index.html", "<!doctype html><title>UI test</title>"), (bad ?? "ui/app.js", "") })
    { using var writer = new StreamWriter(archive.CreateEntry(name).Open(), Encoding.UTF8); writer.Write(text); }
    return path;
}
var store = new ModuleStore(Path.Combine(root, "modules"));
await using var host = new HostConnection(store);
try
{
    await store.EnsureAsync(); string credentials = store.LoadIndex()["token"]!.GetValue<string>();
    await store.EnsureAsync(); Check(store.LoadIndex()["token"]!.GetValue<string>() == credentials, "Credential changed without a host restart.");
    var imported = await store.ImportAsync(fixture);
    Check(!imported.Enabled && imported.Supported && imported.Ui.Length > 0, "Original ZIP import did not preserve platform/UI support or default-disabled state.");
    using (var archive = ZipFile.OpenRead(fixture))
    using (var source = archive.GetEntry(imported.Manifest["libraries"]!["windows-x64"]!.GetValue<string>())!.Open())
        Check(SHA256.HashData(source).SequenceEqual(SHA256.HashData(File.ReadAllBytes(Path.Combine(imported.Directory, imported.Manifest["libraries"]!["windows-x64"]!.GetValue<string>())))), "Published native library changed during import.");
    foreach (string bad in new[] { "../escape.dll", "ui/../../escape.dll", "C:/escape.dll", "ui/CON.js" })
        await Reject(() => store.ImportAsync(Zip("example.invalid", bad)), "Accepted unsafe ZIP path " + bad);
    string ui = Zip("example.ui"); await store.ImportAsync(ui);
    await store.MoveAsync("example.ui", -1); Check(store.Modules()[0].Id == "example.ui", "Reordering failed.");
    await store.MoveAsync("example.ui", 1);
    await store.SetEnabledAsync(imported.Id, true);
    await host.StartOfflineAsync(executable);
    async Task<JsonNode> WaitStatus(string id, Func<JsonNode, bool> predicate)
    {
        for (int i = 0; i < 100; i++) { var status = await host.RequestAsync("status", id); if (status["module"] is JsonNode module && predicate(module)) return status; await Task.Delay(100); }
        throw new Exception("Module state timed out: " + id);
    }
    var ready = await WaitStatus(imported.Id, n => n["status"]?.GetValue<string>() == "ready");
    Check(ready["mode"]!.GetValue<string>() == "offline", "Offline connection incorrectly described as a game.");
    await host.StartOfflineAsync(executable); Check(host.OwnsOfflineHost, "Repeated start replaced or lost the product host.");
    var config = new JsonObject { ["label"] = "EML compatibility", ["unknown"] = new JsonArray(1, "kept", new JsonObject { ["value"] = true }) };
    await WebBridge.DispatchAsync(store, host, imported.Id, "saveConfig", config, "save");
    Check(JsonNode.DeepEquals(config, await WebBridge.DispatchAsync(store, host, imported.Id, "readConfig", null, "read")), "Old bridge configuration lost author-owned fields.");
    await host.RequestAsync("configure", imported.Id, config);
    await WebBridge.DispatchAsync(store, host, imported.Id, "send", new JsonArray("old module", 42), "compat-proof");
    JsonNode? reply = null; bool receivedEvent = false;
    for (int i = 0; i < 50 && reply is null; i++)
    {
        var messages = (await host.RequestAsync("poll", imported.Id))["messages"]!.AsArray();
        receivedEvent |= messages.Any(n => n?["kind"]?.GetValue<string>() == "event");
        reply = messages.FirstOrDefault(n => n?["kind"]?.GetValue<string>() == "reply" && n["request_id"]?.GetValue<string>() == "compat-proof");
        if (reply is null) await Task.Delay(50);
    }
    Check(reply?["body"]?["configuration"]?["label"]?.GetValue<string>() == "EML compatibility", "Original DLL did not receive the current configuration.");
    Check(reply?["body"]?["echo"]?[1]?.GetValue<int>() == 42 && receivedEvent, "Original reply/event delivery failed.");
    await store.SetEnabledAsync(imported.Id, false); await WaitStatus(imported.Id, n => n["status"]?.GetValue<string>() == "disabled");
    await Reject(() => host.RequestAsync("send", imported.Id, new JsonObject(), "disabled"), "Disabled module accepted a message.");
    await store.SetEnabledAsync(imported.Id, true); await WaitStatus(imported.Id, n => n["status"]?.GetValue<string>() == "ready");
    var upgrade = await store.ImportAsync(fixture);
    Check(upgrade.Enabled && upgrade.Generation != imported.Generation && JsonNode.DeepEquals(upgrade.State["configuration"], config), "Upgrade lost enabled state or raw configuration.");
    await WaitStatus(imported.Id, n => n["restart_required"]?.GetValue<bool>() == true);
    Check(Directory.Exists(imported.Directory), "Upgrade destroyed a loaded generation.");
    await store.RemoveAsync(imported.Id); Check(Directory.Exists(upgrade.Directory), "Removal destroyed a potentially resident generation.");
    Check(!store.Modules().Any(n => n.Id == imported.Id), "Removed module remained in the active index.");
    await host.StopOfflineAsync(); Check(!host.OwnsOfflineHost, "Offline Host failed to stop.");
    Console.WriteLine($"PASS {checks} product checks: original ZIP/DLL, raw config, lifecycle, messages/events, upgrade pinning and cleanup boundaries.");
}
finally { await host.StopOfflineAsync(); Directory.Delete(root, true); }
