using Microsoft.Win32;
using System.Security;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace EModLoader.Core;

public sealed class GamePathSettings(string? dataRoot = null)
{
    private readonly string directory = Path.GetFullPath(dataRoot ?? ProductPaths.DataRoot);
    private readonly SemaphoreSlim gate = new(1, 1);
    public string FilePath => Path.Combine(directory, "launcher.json");
    private JsonObject ReadSettings()
    {
        try
        {
            if (!File.Exists(FilePath) || new FileInfo(FilePath).Length > 16384) return new();
            return JsonNode.Parse(File.ReadAllText(FilePath)) as JsonObject ?? new();
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException) { return new(); }
    }
    public string Load() => ReadSettings()["game_executable_path"] is JsonValue value && value.TryGetValue<string>(out var path) ? path : "";
    public bool LoadAllowXInputConflict() => ReadSettings()["allow_xinput_conflict"] is JsonValue value && value.TryGetValue<bool>(out var enabled) && enabled;
    public Task SaveAsync(string path)
    {
        string executable = GamePathDiscovery.Resolve(path) ?? throw new InvalidDataException("请选择有效的 Endfield.exe。");
        return UpdateAsync(settings => settings["game_executable_path"] = executable);
    }
    public Task SaveAllowXInputConflictAsync(bool enabled) => UpdateAsync(settings => settings["allow_xinput_conflict"] = enabled);
    private async Task UpdateAsync(Action<JsonObject> update)
    {
        await gate.WaitAsync();
        try
        {
            var settings = ReadSettings(); settings["schema"] = 1; update(settings);
            Directory.CreateDirectory(directory);
            string pending = Path.Combine(directory, Guid.NewGuid() + ".pending");
            try
            {
                await File.WriteAllTextAsync(pending, settings.ToJsonString());
                File.Move(pending, FilePath, true);
            }
            finally { if (File.Exists(pending)) File.Delete(pending); }
        }
        finally { gate.Release(); }
    }
}

public static class GamePathDiscovery
{
    private static readonly string[] ChildDirectories = ["Endfield Game", "Endfield", "Game", "Games/Endfield", "Games/Endfield Game"];
    public static string? Resolve(string candidate)
    {
        if (string.IsNullOrWhiteSpace(candidate)) return null;
        try
        {
            string path = Path.GetFullPath(candidate.Trim().Trim('"'));
            if (File.Exists(path) && Path.GetFileName(path).Equals("Endfield.exe", StringComparison.OrdinalIgnoreCase)) return path;
            if (!Directory.Exists(path)) return null;
            foreach (string relative in new[] { "" }.Concat(ChildDirectories))
            {
                string executable = Path.Combine(path, relative, "Endfield.exe");
                if (File.Exists(executable)) return Path.GetFullPath(executable);
            }
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or ArgumentException or NotSupportedException) { }
        return null;
    }
    public static IReadOnlyList<string> Discover(string preferred = "") => Discover(preferred, RegistryLocations(), FixedRoots());
    internal static IReadOnlyList<string> Discover(string preferred, IEnumerable<string> registered, IEnumerable<string> roots)
    {
        var found = new List<string>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        void Add(string path) { if (Resolve(path) is string executable && seen.Add(executable)) found.Add(executable); }
        Add(preferred);
        foreach (string path in registered) Add(path);
        foreach (string root in roots)
        {
            Add(root);
            foreach (string folder in new[] { "Games", "Game", "Program Files", "Program Files (x86)", "GRYPHLINE", "Hypergryph" })
                Add(Path.Combine(root, folder));
            foreach (string container in new[] { root, Path.Combine(root, "Games"), Path.Combine(root, "Game"),
                Path.Combine(root, "Program Files"), Path.Combine(root, "Program Files (x86)") })
            {
                foreach (string directory in Directories(container))
                {
                    if (!NamedInstall(Path.GetFileName(directory))) continue;
                    Add(directory);
                    foreach (string child in Directories(directory))
                        if (NamedInstall(Path.GetFileName(child))) Add(child);
                }
            }
        }
        return found;
    }
    private static bool NamedInstall(string name) => name.Contains("endfield", StringComparison.OrdinalIgnoreCase) ||
        name.Contains("终末地", StringComparison.OrdinalIgnoreCase) || name.Contains("gryphline", StringComparison.OrdinalIgnoreCase) ||
        name.Contains("hypergryph", StringComparison.OrdinalIgnoreCase);
    private static IEnumerable<string> Directories(string root)
    {
        try
        {
            return Directory.EnumerateDirectories(root, "*", new EnumerationOptions {
                IgnoreInaccessible = true, AttributesToSkip = FileAttributes.ReparsePoint, RecurseSubdirectories = false
            }).Take(512).ToArray();
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or ArgumentException) { return []; }
    }
    private static IEnumerable<string> FixedRoots()
    {
        var roots = new List<string>();
        foreach (DriveInfo drive in DriveInfo.GetDrives())
        {
            try { if (drive.DriveType == DriveType.Fixed && drive.IsReady) roots.Add(drive.RootDirectory.FullName); }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException) { }
        }
        return roots;
    }
    private static IEnumerable<string> RegistryLocations()
    {
        var locations = new List<string>();
        if (!OperatingSystem.IsWindows()) return locations;
        foreach (RegistryHive hive in new[] { RegistryHive.CurrentUser, RegistryHive.LocalMachine })
        foreach (RegistryView view in new[] { RegistryView.Registry64, RegistryView.Registry32 })
        {
            try
            {
                using var baseKey = RegistryKey.OpenBaseKey(hive, view);
                using var uninstall = baseKey.OpenSubKey(@"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall");
                if (uninstall is null) continue;
                foreach (string name in uninstall.GetSubKeyNames().Take(4096))
                {
                    using var product = uninstall.OpenSubKey(name);
                    string label = product?.GetValue("DisplayName") as string ?? "";
                    if (!label.Contains("Endfield", StringComparison.OrdinalIgnoreCase) && !label.Contains("终末地", StringComparison.OrdinalIgnoreCase)) continue;
                    if (product?.GetValue("InstallLocation") is string location) locations.Add(location);
                    if (product?.GetValue("DisplayIcon") is string icon)
                    {
                        string path = icon.Trim();
                        int quote = path.StartsWith('"') ? path.IndexOf('"', 1) : -1;
                        int comma = path.LastIndexOf(',');
                        if (quote > 0) path = path[1..quote];
                        else if (comma >= 0 && int.TryParse(path[(comma + 1)..], out _)) path = path[..comma];
                        locations.Add(path);
                        if (Path.GetDirectoryName(path) is string parent) locations.Add(parent);
                    }
                }
            }
            catch (Exception error) when (error is IOException or UnauthorizedAccessException or SecurityException or ArgumentException) { }
        }
        return locations;
    }
}
