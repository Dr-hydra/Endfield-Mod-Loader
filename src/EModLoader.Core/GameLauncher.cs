using System.Diagnostics;

namespace EModLoader.Core;
public static class GameLauncher
{
    public static async Task<int> LaunchAsync(ModuleStore store, string game)
    {
        await store.EnsureAsync();
        string launcher = Path.Combine(AppContext.BaseDirectory, "runtime", "EModLoader.Launcher.exe");
        string library = Path.Combine(AppContext.BaseDirectory, "runtime", "EModLoader.Host.dll");
        if (!File.Exists(launcher) || !File.Exists(library)) throw new FileNotFoundException("未找到游戏启动组件。");
        if (File.Exists(Path.Combine(Path.GetDirectoryName(Path.GetFullPath(game))!, "xinput1_4.dll")))
            throw new InvalidOperationException("游戏目录已有 XInput 加载器，请先处理启动方式冲突。");
        var info = new ProcessStartInfo(launcher) { UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true };
        foreach (string arg in new[] { "--game", Path.GetFullPath(game), "--index", store.IndexPath, "--host", library }) info.ArgumentList.Add(arg);
        using var process = Process.Start(info) ?? throw new InvalidOperationException("无法启动游戏加载器。");
        var output = process.StandardOutput.ReadToEndAsync(); var errors = process.StandardError.ReadToEndAsync();
        await process.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(45));
        if (process.ExitCode != 0) throw new InvalidOperationException("加载器启动失败：" + await errors);
        return int.Parse((await output).Trim(), System.Globalization.CultureInfo.InvariantCulture);
    }
}
