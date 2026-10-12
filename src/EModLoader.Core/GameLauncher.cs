using System.ComponentModel;
using System.Diagnostics;
using System.IO.Pipes;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EModLoader.Core;

internal sealed record LaunchReport(
    [property: JsonPropertyName("protocol")] string Protocol,
    [property: JsonPropertyName("exit_code")] int ExitCode,
    [property: JsonPropertyName("win32_error")] int Win32Error,
    [property: JsonPropertyName("process_id")] int ProcessId,
    [property: JsonPropertyName("error")] string Error);

public static class GameLauncher
{
    public static Task<int> LaunchAsync(ModuleStore store, string game, Action<string>? progress = null, bool allowXInputConflict = false) =>
        LaunchAsync(store, game, ProductPaths.RuntimeDirectory, progress, allowXInputConflict: allowXInputConflict);

    internal static async Task<int> LaunchAsync(ModuleStore store, string game, string runtimeDirectory,
        Action<string>? progress = null, Func<bool, Task<LaunchReport>>? run = null, bool allowXInputConflict = false)
    {
        await store.EnsureAsync();
        string launcher = Path.GetFullPath(Path.Combine(runtimeDirectory, "EModLoader.Launcher.exe"));
        string library = Path.GetFullPath(Path.Combine(runtimeDirectory, "EModLoader.Host.dll"));
        game = Path.GetFullPath(game);
        if (!File.Exists(game)) throw new FileNotFoundException("游戏路径已失效，请重新选择 Endfield.exe。", game);
        if (!File.Exists(launcher) || !File.Exists(library)) throw new FileNotFoundException("未找到游戏启动组件。");
        bool foreignXInput = File.Exists(Path.Combine(Path.GetDirectoryName(game)!, "xinput1_4.dll")) && !ProxyInstallation.IsOwned(game);
        if (foreignXInput && !allowXInputConflict)
            throw new InvalidOperationException("游戏目录已有其他 XInput DLL，可勾选“保留其他 XInput DLL 并加载 EML”后启动。");
        if (foreignXInput) progress?.Invoke("保留现有 XInput DLL，并显式注入 EML Host。");
        return await LaunchWithElevationAsync(run ?? (elevated => RunAsync(launcher, game, store.IndexPath, library, elevated,
            allowXInputConflict: allowXInputConflict)), progress);
    }

    internal static async Task<int> LaunchWithElevationAsync(Func<bool, Task<LaunchReport>> run, Action<string>? progress)
    {
        var report = await RunWithElevationAsync(run, progress, false);
        if (report.ProcessId <= 0) throw new InvalidOperationException("启动组件未返回有效的游戏进程。");
        return report.ProcessId;
    }

    public static async Task DeployProxyAsync(ModuleStore store, string game, bool install, Action<string>? progress = null)
    {
        await store.EnsureAsync();
        string directory = ProductPaths.RuntimeDirectory;
        string launcher = Path.Combine(directory, "EModLoader.Launcher.exe"), host = Path.Combine(directory, "EModLoader.Host.dll"),
            proxy = Path.Combine(directory, "EModLoader.XInputProxy.dll");
        if (!File.Exists(launcher) || !File.Exists(host) || !File.Exists(proxy)) throw new FileNotFoundException("未找到 DLL 自启动组件。");
        string executable = GamePathDiscovery.Resolve(game) ?? throw new InvalidDataException("请选择有效的 Endfield.exe。");
        string existing = Path.Combine(Path.GetDirectoryName(executable)!, "xinput1_4.dll");
        if (File.Exists(existing) && !ProxyInstallation.IsOwned(executable)) throw new InvalidOperationException("游戏目录已有其他工具的 XInput DLL，无法覆盖或卸载。");
        await RunWithElevationAsync(elevated => RunAsync(launcher, executable, store.IndexPath, host, elevated,
            install ? "install-proxy" : "remove-proxy", proxy), progress, true);
    }

    internal static async Task<LaunchReport> RunWithElevationAsync(Func<bool, Task<LaunchReport>> run, Action<string>? progress, bool deployment)
    {
        LaunchReport report;
        try { report = await run(false); }
        catch (Win32Exception error) when (error.NativeErrorCode == 740)
        { report = new("eml.launch.v1", 4, 740, 0, error.Message); }
        if (report.Protocol != "eml.launch.v1") throw new InvalidDataException("启动组件返回了无效的响应。");
        if (report.Win32Error == 740 || deployment && report.Win32Error is 5 or 1314)
        {
            progress?.Invoke(deployment ? "更改 DLL 自启动需要管理员权限，请确认 Windows 的权限请求…" : "游戏需要管理员权限，请确认 Windows 的权限请求…");
            await Task.Yield();
            try { report = await run(true); }
            catch (Win32Exception error) when (error.NativeErrorCode == 1223)
            { throw new OperationCanceledException(deployment ? "已取消管理员权限请求，DLL 自启动未更改。" : "已取消管理员权限请求，游戏未启动。", error); }
        }
        if (report.Protocol != "eml.launch.v1") throw new InvalidDataException("启动组件返回了无效的响应。");
        if (report.ExitCode != 0)
            throw new InvalidOperationException(report.Win32Error == 740
                ? "管理员权限不足，游戏仍无法启动。"
                : "加载器启动失败：" + report.Error);
        return report;
    }

    private static async Task<LaunchReport> RunAsync(string launcher, string game, string index, string library, bool elevated,
        string? operation = null, string? proxy = null, bool allowXInputConflict = false)
    {
        if (!OperatingSystem.IsWindows()) throw new PlatformNotSupportedException("游戏启动仅支持 Windows。");
        string pipeName = "EML.Launch." + Guid.NewGuid().ToString("N");
        await using var pipe = new NamedPipeServerStream(pipeName, PipeDirection.In, 1, PipeTransmissionMode.Byte,
            PipeOptions.Asynchronous | PipeOptions.CurrentUserOnly);
        using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(elevated ? 120 : 50));
        Task connected = pipe.WaitForConnectionAsync(timeout.Token);
        try
        {
            var info = new ProcessStartInfo(launcher) {
                UseShellExecute = elevated, CreateNoWindow = !elevated, WindowStyle = ProcessWindowStyle.Hidden,
                WorkingDirectory = Path.GetDirectoryName(launcher)!
            };
            if (elevated) info.Verb = "runas";
            foreach (string argument in new[] { "--game", game, "--index", index, "--host", library, "--result-pipe", pipeName })
                info.ArgumentList.Add(argument);
            if (allowXInputConflict) { info.ArgumentList.Add("--allow-xinput-conflict"); info.ArgumentList.Add("true"); }
            if (operation is not null) { info.ArgumentList.Add("--operation"); info.ArgumentList.Add(operation); info.ArgumentList.Add("--proxy"); info.ArgumentList.Add(proxy!); }
            using var process = Process.Start(info) ?? throw new InvalidOperationException("无法启动游戏加载器。");
            Task exited = process.WaitForExitAsync(timeout.Token);
            Task winner = await Task.WhenAny(connected, exited);
            if (winner == exited && !pipe.IsConnected)
                throw new InvalidOperationException($"启动组件在返回结果前退出（代码 {process.ExitCode}）。");
            await connected;
            if (!GetNamedPipeClientProcessId(pipe.SafePipeHandle, out uint client) || client != process.Id)
                throw new InvalidDataException("启动组件连接身份不匹配。");
            byte[] prefix = new byte[4]; await pipe.ReadExactlyAsync(prefix, timeout.Token);
            int length = System.Buffers.Binary.BinaryPrimitives.ReadInt32LittleEndian(prefix);
            if (length is <= 0 or > 16384) throw new InvalidDataException("启动组件响应长度无效。");
            byte[] data = new byte[length]; await pipe.ReadExactlyAsync(data, timeout.Token);
            var report = JsonSerializer.Deserialize<LaunchReport>(data) ?? throw new InvalidDataException("启动组件响应无效。");
            await exited;
            if (report.ExitCode != process.ExitCode) throw new InvalidDataException("启动组件退出状态不一致。");
            return report;
        }
        catch (OperationCanceledException error)
        { throw new TimeoutException("游戏启动超时，请检查游戏进程与启动日志。", error); }
        finally { timeout.Cancel(); }
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool GetNamedPipeClientProcessId(Microsoft.Win32.SafeHandles.SafePipeHandle pipe, out uint processId);
}
