using System.IO.Compression;
using System.Net;
using System.Net.Sockets;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace EModLoader.Core;

public sealed class ModuleStore
{
    private readonly SemaphoreSlim gate = new(1, 1);
    public string Root { get; }
    public string IndexPath => Path.Combine(Root, "index.json");
    public ModuleStore(string? root = null) => Root = Path.GetFullPath(root ?? ProductPaths.ModulesRoot);
    private static JsonObject NewIndex()
    {
        var listener = new TcpListener(IPAddress.Loopback, 0); listener.Start();
        int port = ((IPEndPoint)listener.LocalEndpoint).Port; listener.Stop();
        return new JsonObject { ["schema"] = 1, ["port"] = port,
            ["token"] = Convert.ToHexString(RandomNumberGenerator.GetBytes(32)).ToLowerInvariant(), ["modules"] = new JsonArray() };
    }
    public JsonObject LoadIndex()
    {
        if (!File.Exists(IndexPath)) return NewIndex();
        if (new FileInfo(IndexPath).Length > 1024 * 1024) throw new InvalidDataException("模块索引超过 1 MiB。");
        var index = JsonNode.Parse(File.ReadAllText(IndexPath))?.AsObject() ?? throw new InvalidDataException("模块索引无效。");
        if (index["schema"]?.GetValue<int>() != 1 || index["port"]?.GetValue<int>() is not int port || port is < 1024 or > 65535 ||
            index["token"]?.GetValue<string>() is not string token || !Regex.IsMatch(token, @"\A[a-f0-9]{64}\z") ||
            index["modules"] is not JsonArray modules || modules.Count > 128) throw new InvalidDataException("模块索引格式无效。");
        var ids = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var node in modules)
        {
            var state = node!.AsObject(); string id = state["id"]!.GetValue<string>(); ModulePackage.ValidateId(id);
            if (!ids.Add(id) || !Guid.TryParseExact(state["generation"]!.GetValue<string>(), "D", out _) || state["configuration"] is not JsonObject)
                throw new InvalidDataException("模块索引记录无效。");
            _ = state["enabled"]!.GetValue<bool>();
            string expected = Path.GetFullPath(Path.Combine(Root, "packages", state["generation"]!.GetValue<string>()));
            if (!string.Equals(Path.GetFullPath(state["directory"]!.GetValue<string>()), expected, StringComparison.OrdinalIgnoreCase))
                throw new InvalidDataException("模块目录不属于本库。");
        }
        return index;
    }
    private async Task WriteAsync(JsonObject index)
    {
        Directory.CreateDirectory(Root); string content = index.ToJsonString();
        if (Encoding.UTF8.GetByteCount(content) > 1024 * 1024) throw new InvalidDataException("模块配置超过 1 MiB。");
        string pending = Path.Combine(Root, Guid.NewGuid() + ".pending");
        try { await File.WriteAllTextAsync(pending, content, new UTF8Encoding(false)); File.Move(pending, IndexPath, true); }
        finally { if (File.Exists(pending)) File.Delete(pending); }
    }
    public async Task EnsureAsync()
    {
        await gate.WaitAsync();
        try { if (!File.Exists(IndexPath)) await WriteAsync(NewIndex()); else _ = LoadIndex(); }
        finally { gate.Release(); }
    }
    public IReadOnlyList<InstalledModule> Modules()
    {
        var result = new List<InstalledModule>();
        foreach (var node in LoadIndex()["modules"]!.AsArray())
        {
            var state = node!.AsObject();
            try { result.Add(new InstalledModule(state, ModulePackage.ReadManifest(state["directory"]!.GetValue<string>()))); }
            catch (Exception error) when (error is IOException or InvalidDataException or InvalidOperationException or ArgumentException or NullReferenceException or System.Text.Json.JsonException)
            { result.Add(new InstalledModule(state, new JsonObject { ["name"] = state["id"]!.GetValue<string>() }, error.Message)); }
        }
        return result;
    }
    public InstalledModule Find(string id) => Modules().FirstOrDefault(record => record.Id == id) ?? throw new InvalidOperationException("模块已移除。");
    private async Task ChangeAsync(Action<JsonObject, JsonArray> change)
    {
        await gate.WaitAsync();
        try { var index = LoadIndex(); change(index, index["modules"]!.AsArray()); await WriteAsync(index); }
        finally { gate.Release(); }
    }
    public Task SetEnabledAsync(string id, bool enabled) => ChangeAsync((_, modules) =>
    {
        var state = modules.First(m => m!["id"]!.GetValue<string>() == id)!.AsObject();
        if (enabled && !new InstalledModule(state, ModulePackage.ReadManifest(state["directory"]!.GetValue<string>())).Supported)
            throw new InvalidOperationException("此包不包含 Windows 原生库或网页。");
        state["enabled"] = enabled;
    });
    public Task SaveConfigurationAsync(string id, JsonObject configuration) => ChangeAsync((_, modules) =>
        modules.First(m => m!["id"]!.GetValue<string>() == id)!["configuration"] = configuration.DeepClone());
    public Task MoveAsync(string id, int direction) => ChangeAsync((_, modules) =>
    {
        int from = modules.Select(m => m!["id"]!.GetValue<string>()).ToList().IndexOf(id), to = from + direction;
        if (from < 0 || to < 0 || to >= modules.Count) return;
        var record = modules[from]; modules.RemoveAt(from); modules.Insert(to, record);
    });
    public Task RemoveAsync(string id) => ChangeAsync((index, modules) =>
    {
        var record = modules.First(m => m!["id"]!.GetValue<string>() == id)!;
        (index["retired_directories"] ??= new JsonArray()).AsArray().Add(record["directory"]!.GetValue<string>());
        modules.Remove(record);
    });
    public async Task<InstalledModule> ImportAsync(string archive)
    {
        if (new FileInfo(archive).Length > ModulePackage.ArchiveLimit) throw new InvalidDataException("ZIP 超过 256 MiB。");
        string generation = Guid.NewGuid().ToString(), directory = Path.Combine(Root, "packages", generation);
        Directory.CreateDirectory(directory); bool published = false;
        try
        {
            using var zip = ZipFile.OpenRead(archive);
            if (zip.Entries.Count > 4096) throw new InvalidDataException("ZIP 条目超过 4096。");
            var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase); long declared = 0, actual = 0;
            foreach (var entry in zip.Entries)
            {
                bool folder = entry.FullName.EndsWith('/'); string relative = ModulePackage.RelativePath(folder ? entry.FullName[..^1] : entry.FullName);
                if (!names.Add(relative) || ((entry.ExternalAttributes >> 16) & 0xF000) == 0xA000 || entry.Length > ModulePackage.EntryLimit ||
                    (declared += entry.Length) > ModulePackage.ArchiveLimit) throw new InvalidDataException("ZIP 包含重复路径、链接或过大的内容。");
                string destination = Path.Combine(directory, relative);
                if (folder) { Directory.CreateDirectory(destination); continue; }
                Directory.CreateDirectory(Path.GetDirectoryName(destination)!);
                await using var input = entry.Open(); await using var output = File.Create(destination);
                byte[] buffer = new byte[65536]; long copied = 0; int count;
                while ((count = await input.ReadAsync(buffer)) > 0)
                {
                    copied += count; actual += count;
                    if (copied > ModulePackage.EntryLimit || actual > ModulePackage.ArchiveLimit) throw new InvalidDataException("ZIP 解包内容超过上限。");
                    await output.WriteAsync(buffer.AsMemory(0, count));
                }
                if (copied != entry.Length) throw new InvalidDataException("ZIP 条目长度不一致。");
            }
            if (!zip.Entries.Any(e => e.FullName == "module.json")) throw new InvalidDataException("ZIP 根目录必须包含 module.json。");
            var manifest = ModulePackage.ReadManifest(directory); string id = manifest["id"]!.GetValue<string>(); JsonObject? installed = null;
            await ChangeAsync((index, modules) =>
            {
                var previous = modules.FirstOrDefault(m => m!["id"]!.GetValue<string>().Equals(id, StringComparison.OrdinalIgnoreCase));
                if (previous is not null && previous["id"]!.GetValue<string>() != id) throw new InvalidDataException("模块 ID 大小写发生变化。");
                if (previous is null && modules.Count >= 128) throw new InvalidDataException("模块数量超过 128。");
                installed = new JsonObject { ["id"] = id, ["enabled"] = previous?["enabled"]?.GetValue<bool>() ?? false,
                    ["directory"] = directory, ["generation"] = generation,
                    ["configuration"] = previous?["configuration"]?.DeepClone() ?? manifest["default_configuration"]?.DeepClone() ?? new JsonObject() };
                if (!new InstalledModule(installed, manifest).Supported) installed["enabled"] = false;
                if (previous is not null)
                {
                    int position = modules.IndexOf(previous); (index["retired_directories"] ??= new JsonArray()).AsArray().Add(previous["directory"]!.GetValue<string>());
                    modules.RemoveAt(position); modules.Insert(position, installed);
                }
                else modules.Add(installed);
            });
            published = true; return new InstalledModule(installed!, manifest);
        }
        finally
        {
            // This directory is a newly-created UUID beneath this store, and only
            // unpublished imports can reach cleanup. Loaded generations are retained.
            if (!published && Directory.Exists(directory)) Directory.Delete(directory, true);
        }
    }
}
