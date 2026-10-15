using Microsoft.Win32;
using System.Runtime.InteropServices;
using System.Runtime.InteropServices.ComTypes;
using System.Security;
using System.Text;
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
    public string LoadOfficialParent() => ReadSettings()["official_parent"] is JsonValue value && value.TryGetValue<string>(out var path) ? path : "";
    public Task SaveOfficialParentAsync(string? path) => UpdateAsync(settings =>
    {
        if (string.IsNullOrWhiteSpace(path))
            settings.Remove("official_parent");
        else
            settings["official_parent"] = path.Trim();
    });
    public bool LoadBypassOfficialLauncher() => ReadSettings()["bypass_official_launcher"] is JsonValue value && value.TryGetValue<bool>(out var enabled) && enabled;
    public Task SaveBypassOfficialLauncherAsync(bool enabled) => UpdateAsync(settings => settings["bypass_official_launcher"] = enabled);
    public IReadOnlyList<string> LoadOfficialLauncherHistory()
    {
        var settings = ReadSettings();
        if (settings["official_launcher_history"] is JsonArray array)
        {
            var list = new List<string>();
            foreach (var node in array)
            {
                if (node is JsonValue val && val.TryGetValue<string>(out var str) && !string.IsNullOrWhiteSpace(str))
                    list.Add(str);
            }
            return list;
        }
        return [];
    }
    public Task SaveOfficialLauncherHistoryAsync(IEnumerable<string> history) => UpdateAsync(settings =>
    {
        var array = new JsonArray();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (var path in history)
        {
            if (!string.IsNullOrWhiteSpace(path) && seen.Add(path))
                array.Add(path);
        }
        settings["official_launcher_history"] = array;
    });
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

public static class OfficialLauncherDiscovery
{
    public static string Resolve(string candidate)
    {
        if (string.IsNullOrWhiteSpace(candidate)) return string.Empty;
        string trimmed = candidate.Trim().Trim('"');
        if (trimmed.EndsWith(".lnk", StringComparison.OrdinalIgnoreCase) && File.Exists(trimmed))
        {
            string? target = ResolveShortcutTarget(trimmed);
            if (!string.IsNullOrWhiteSpace(target) && File.Exists(target))
            {
                trimmed = target;
            }
        }

        if (File.Exists(trimmed) && Path.GetFileName(trimmed).Equals("Games.exe", StringComparison.OrdinalIgnoreCase))
        {
            return Path.GetFullPath(trimmed);
        }

        string searchDir = Directory.Exists(trimmed) ? trimmed : (Path.GetDirectoryName(trimmed) ?? string.Empty);
        if (!string.IsNullOrWhiteSpace(searchDir) && Directory.Exists(searchDir))
        {
            string? best = FindBestGamesExecutable(searchDir);
            if (!string.IsNullOrWhiteSpace(best))
            {
                return best;
            }
        }

        if (File.Exists(trimmed))
        {
            return Path.GetFullPath(trimmed);
        }

        return trimmed;
    }

    public static string FormatDisplayName(string path)
    {
        if (string.IsNullOrWhiteSpace(path)) return string.Empty;
        string normalized = Path.GetFullPath(path.Trim());
        string serverTag = "[自定义]";
        if (normalized.Contains("Hypergryph", StringComparison.OrdinalIgnoreCase) ||
            normalized.Contains("鹰角", StringComparison.OrdinalIgnoreCase))
        {
            serverTag = "[国服]";
        }
        else if (normalized.Contains("GRYPHLINK", StringComparison.OrdinalIgnoreCase) ||
                 normalized.Contains("Gryphline", StringComparison.OrdinalIgnoreCase))
        {
            serverTag = "[国际服]";
        }
        return $"{serverTag} {normalized}";
    }

    public static string DiscoverDefault(string preferredPath = "")
    {
        if (!string.IsNullOrWhiteSpace(preferredPath))
        {
            string resolved = Resolve(preferredPath);
            if (File.Exists(resolved))
            {
                return resolved;
            }
        }

        var all = DiscoverAll();
        return all.Count > 0 ? all[0] : string.Empty;
    }

    public static List<string> DiscoverAll()
    {
        if (!OperatingSystem.IsWindows()) return [];
        var roots = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        // 1. HKCR protocols
        foreach (string protocol in new[] { "hypergryphlauncher", "gryphlinklauncher" })
        {
            try
            {
                string? command = Registry.GetValue($@"HKEY_CLASSES_ROOT\{protocol}\shell\open\command", null, null) as string;
                if (!string.IsNullOrWhiteSpace(command))
                {
                    string exe = ExtractExecutablePath(command);
                    if (!string.IsNullOrWhiteSpace(exe))
                    {
                        string? dir = Path.GetDirectoryName(exe);
                        if (!string.IsNullOrWhiteSpace(dir) && Directory.Exists(dir))
                        {
                            roots.Add(dir);
                        }
                    }
                }
            }
            catch { }
        }

        // 2. Registry Uninstall keys
        string[] searchHives = [
            @"SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall",
            @"SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall"
        ];
        string[] targetNames = ["鹰角启动器", "Hypergryph Launcher", "GRYPHLINK", "Gryphlink", "Gryphline Launcher"];

        foreach (RegistryKey baseKey in new[] { Registry.CurrentUser, Registry.LocalMachine })
        {
            foreach (string uninstallPath in searchHives)
            {
                RegistryKey? key = null;
                try
                {
                    key = baseKey.OpenSubKey(uninstallPath);
                    if (key == null) continue;
                    foreach (string subkeyName in key.GetSubKeyNames())
                    {
                        using RegistryKey? subkey = key.OpenSubKey(subkeyName);
                        if (subkey == null) continue;
                        string? displayName = subkey.GetValue("DisplayName") as string;
                        if (string.IsNullOrWhiteSpace(displayName)) continue;
                        if (targetNames.Any(name => displayName.Contains(name, StringComparison.OrdinalIgnoreCase)))
                        {
                            string? installLocation = subkey.GetValue("InstallLocation") as string;
                            if (!string.IsNullOrWhiteSpace(installLocation) && Directory.Exists(installLocation))
                            {
                                roots.Add(installLocation);
                            }
                        }
                    }
                }
                catch { }
                finally
                {
                    key?.Dispose();
                }
            }
        }

        // 3. Fixed common environment paths
        string[] commonFolders = ["Hypergryph Launcher", "GRYPHLINK"];
        string[] envRoots = [
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFilesX86),
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)
        ];
        foreach (string envRoot in envRoots)
        {
            if (string.IsNullOrWhiteSpace(envRoot)) continue;
            foreach (string folder in commonFolders)
            {
                string combined = Path.Combine(envRoot, folder);
                if (Directory.Exists(combined))
                {
                    roots.Add(combined);
                }
            }
        }

        // 4. From collected roots, find best Games.exe
        var candidates = new List<(string Path, Version Version)>();
        var seen = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        foreach (string root in roots)
        {
            string? found = FindBestGamesExecutable(root);
            if (!string.IsNullOrWhiteSpace(found) && seen.Add(found))
            {
                string dirName = Path.GetFileName(Path.GetDirectoryName(found) ?? string.Empty);
                Version.TryParse(dirName, out Version? ver);
                candidates.Add((found, ver ?? new Version(0, 0, 0)));
            }
        }

        return candidates.OrderByDescending(c => c.Version).Select(c => c.Path).ToList();
    }

    private static string? FindBestGamesExecutable(string rootDir)
    {
        if (!Directory.Exists(rootDir)) return null;

        var candidates = new List<(string Path, Version Version)>();

        string rootGames = Path.Combine(rootDir, "Games.exe");
        if (File.Exists(rootGames))
        {
            candidates.Add((Path.GetFullPath(rootGames), new Version(0, 0, 0)));
        }

        try
        {
            foreach (string subDir in Directory.EnumerateDirectories(rootDir))
            {
                string dirName = Path.GetFileName(subDir);
                if (Version.TryParse(dirName, out Version? ver))
                {
                    string candidate = Path.Combine(subDir, "Games.exe");
                    if (File.Exists(candidate))
                    {
                        candidates.Add((Path.GetFullPath(candidate), ver));
                    }
                }
            }
        }
        catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
        {
        }

        if (candidates.Count == 0) return null;
        return candidates.OrderByDescending(c => c.Version).First().Path;
    }

    private static string ExtractExecutablePath(string command)
    {
        if (string.IsNullOrWhiteSpace(command)) return string.Empty;
        string trimmed = command.Trim();
        if (trimmed.StartsWith("\""))
        {
            int nextQuote = trimmed.IndexOf('"', 1);
            if (nextQuote > 1) return trimmed[1..nextQuote];
        }
        int space = trimmed.IndexOf(' ');
        return space > 0 ? trimmed[..space] : trimmed;
    }

    private static string? ResolveShortcutTarget(string shortcutPath)
    {
        if (!OperatingSystem.IsWindows()) return null;
        if (string.IsNullOrWhiteSpace(shortcutPath) || !File.Exists(shortcutPath)) return null;
        object shellLinkObject = new ShellLink();
        try
        {
            var persistFile = (IPersistFile)shellLinkObject;
            persistFile.Load(shortcutPath, 0);
            var shellLink = (IShellLinkW)shellLinkObject;
            var targetBuilder = new StringBuilder(1024);
            shellLink.GetPath(targetBuilder, targetBuilder.Capacity, nint.Zero, 0);
            string target = targetBuilder.ToString();
            return string.IsNullOrWhiteSpace(target) ? null : target;
        }
        catch
        {
            return null;
        }
        finally
        {
            Marshal.FinalReleaseComObject(shellLinkObject);
        }
    }

    [ComImport]
    [Guid("00021401-0000-0000-C000-000000000046")]
    private sealed class ShellLink { }

    [ComImport]
    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    [Guid("000214F9-0000-0000-C000-000000000046")]
    private interface IShellLinkW
    {
        void GetPath([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder file, int fileLength, nint findData, uint flags);
        void GetIDList(out nint itemIdList);
        void SetIDList(nint itemIdList);
        void GetDescription([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder name, int nameLength);
        void SetDescription([MarshalAs(UnmanagedType.LPWStr)] string name);
        void GetWorkingDirectory([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder directory, int directoryLength);
        void SetWorkingDirectory([MarshalAs(UnmanagedType.LPWStr)] string directory);
        void GetArguments([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder arguments, int argumentsLength);
        void SetArguments([MarshalAs(UnmanagedType.LPWStr)] string arguments);
        void GetHotkey(out short hotkey);
        void SetHotkey(short hotkey);
        void GetShowCmd(out int showCommand);
        void SetShowCmd(int showCommand);
        void GetIconLocation([Out, MarshalAs(UnmanagedType.LPWStr)] StringBuilder iconPath, int iconPathLength, out int iconIndex);
        void SetIconLocation([MarshalAs(UnmanagedType.LPWStr)] string iconPath, int iconIndex);
        void SetRelativePath([MarshalAs(UnmanagedType.LPWStr)] string path, uint reserved);
        void Resolve(nint hwnd, uint flags);
        void SetPath([MarshalAs(UnmanagedType.LPWStr)] string path);
    }
}
