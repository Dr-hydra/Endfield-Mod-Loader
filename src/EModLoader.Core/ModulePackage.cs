using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace EModLoader.Core;

public sealed record InstalledModule(JsonObject State, JsonObject Manifest, string Error = "")
{
    public string Id => State["id"]!.GetValue<string>();
    public string Generation => State["generation"]!.GetValue<string>();
    public string Directory => State["directory"]!.GetValue<string>();
    public string Name => Manifest["name"]?.GetValue<string>() ?? Id;
    public string Author => Manifest["author"]?.GetValue<string>() ?? "";
    public string Version => Manifest["version"]?.GetValue<string>() ?? "";
    public bool Enabled => State["enabled"]!.GetValue<bool>();
    public string Ui => Manifest["ui"]?.GetValue<string>() ?? "";
    public bool Supported => Error.Length == 0 && (Ui.Length > 0 || Manifest["libraries"]?["windows-x64"] is not null);
}

public static class ModulePackage
{
    public const long ArchiveLimit = 256L * 1024 * 1024, EntryLimit = 128L * 1024 * 1024;
    public static void ValidateId(string id)
    {
        if (!Regex.IsMatch(id, @"\A[A-Za-z0-9][A-Za-z0-9_.-]{0,95}\z") ||
            id.StartsWith("betterendfield.", StringComparison.OrdinalIgnoreCase) || id.StartsWith("emodloader.", StringComparison.OrdinalIgnoreCase) ||
            id.Equals("voice.character", StringComparison.OrdinalIgnoreCase)) throw new InvalidDataException("模块 ID 无效或使用了保留名称。");
    }
    public static string RelativePath(string value)
    {
        if (value.Length is 0 or > 240 || value.Contains('\\') || value.Contains(':') || value.StartsWith('/'))
            throw new InvalidDataException("模块包包含不安全的路径。");
        foreach (string part in value.Split('/'))
            if (part.Length == 0 || part is "." or ".." || part.EndsWith('.') || part.EndsWith(' ') || part.Any(char.IsControl) ||
                Regex.IsMatch(part.Split('.')[0], @"\A(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])\z", RegexOptions.IgnoreCase))
                throw new InvalidDataException("模块包包含不安全的路径。");
        return value;
    }
    public static JsonObject ReadManifest(string directory)
    {
        string path = Path.Combine(directory, "module.json");
        if (!File.Exists(path) || new FileInfo(path).Length > 256 * 1024) throw new InvalidDataException("缺少 module.json 或清单过大。");
        var manifest = JsonNode.Parse(File.ReadAllText(path))?.AsObject() ?? throw new InvalidDataException("模块清单无效。");
        if (manifest["format"]?.GetValue<int>() != 1 || manifest["abi"]?.GetValue<int>() != 1) throw new InvalidDataException("仅支持原 BE format 1 / ABI 1 模块包。");
        ValidateId(manifest["id"]!.GetValue<string>());
        foreach (string key in new[] { "name", "author", "version" })
            if (manifest[key]?.GetValue<string>() is not string value || value.Length > 200 || (key == "name" && value.Length == 0))
                throw new InvalidDataException($"模块清单字段 {key} 无效。");
        var libraries = manifest["libraries"]?.AsObject() ?? new JsonObject();
        foreach (var library in libraries)
        {
            string relative = RelativePath(library.Value!.GetValue<string>());
            if (!File.Exists(Path.Combine(directory, relative)) ||
                (library.Key == "windows-x64" && !relative.EndsWith(".dll", StringComparison.OrdinalIgnoreCase)) ||
                (library.Key == "android-arm64" && !relative.EndsWith(".so", StringComparison.Ordinal)))
                throw new InvalidDataException("原生库路径无效。");
        }
        string ui = manifest["ui"]?.GetValue<string>() ?? "";
        if (ui.Length > 0 && (!RelativePath(ui).EndsWith(".html", StringComparison.OrdinalIgnoreCase) || !File.Exists(Path.Combine(directory, ui))))
            throw new InvalidDataException("模块网页入口无效。");
        if (ui.Length == 0 && libraries.Count == 0) throw new InvalidDataException("模块缺少原生库或网页入口。");
        if (manifest["default_configuration"] is not null && manifest["default_configuration"] is not JsonObject)
            throw new InvalidDataException("默认配置必须是 JSON 对象。");
        if (manifest["dependencies"] is JsonArray dependencies)
        {
            if (dependencies.Count > 128) throw new InvalidDataException("模块依赖过多。");
            foreach (var dependency in dependencies)
            {
                string id = dependency!.GetValue<string>(); ValidateId(id);
                if (id == manifest["id"]!.GetValue<string>()) throw new InvalidDataException("模块不能依赖自身。");
            }
        }
        else if (manifest["dependencies"] is not null) throw new InvalidDataException("依赖必须是数组。");
        return manifest;
    }
}
