using System.Text.Json;
using System.Windows;
using System.Windows.Controls;
using EModLoader.Core;
using Microsoft.Web.WebView2.Core;

namespace EModLoader.Desktop;

internal static class PackageSelfCheck
{
    private static readonly string[] Files = ["EModLoader.OfflineHost.exe", "EModLoader.Launcher.exe", "EModLoader.Host.dll", "EModLoader.XInputProxy.dll"];
    internal static async Task<int> RunAsync(string reportPath)
    {
        string path = Path.GetFullPath(reportPath);
        try
        {
            string runtime = ProductPaths.RuntimeDirectory;
            if (!Files.All(file => File.Exists(Path.Combine(runtime, file))))
                throw new FileNotFoundException("运行组件不完整。");
            // Construct WPF controls without opening a window or driving a game.
            var window = new MainWindow();
            var option = (CheckBox)window.FindName("AllowXInputConflict");
            if (option.Visibility != Visibility.Visible || option.IsChecked != new GamePathSettings().LoadAllowXInputConflict())
                throw new InvalidDataException("XInput 开关未正确读取保存的设置。");
            string browserVersion = CoreWebView2Environment.GetAvailableBrowserVersionString();
            if (string.IsNullOrWhiteSpace(browserVersion)) throw new InvalidDataException("WebView2 运行时不可用。");
            var store = new ModuleStore(Path.Combine(Path.GetDirectoryName(path)!, "self-check-library"));
            await using var host = new HostConnection(store);
            await host.StartOfflineAsync();
            var status = await host.RequestAsync("status");
            if (status["mode"]?.GetValue<string>() != "offline") throw new InvalidDataException("离线宿主握手失败。");
            await host.StopOfflineAsync();
            await File.WriteAllTextAsync(path, JsonSerializer.Serialize(new {
                success = true, version = "0.1.2", xinput_option_visible = true, xinput_option_enabled = option.IsChecked == true, xinput_option_matches_settings = true,
                package_file_count = Directory.EnumerateFiles(AppContext.BaseDirectory, "*", SearchOption.AllDirectories).Count(),
                runtime_directory = runtime, runtime_files = Files,
                wpf_controls = true, webview2_loader = true, offline_host_handshake = true, owned_host_stopped = true
            }, new JsonSerializerOptions { WriteIndented = true }));
            return 0;
        }
        catch (Exception error)
        {
            await File.WriteAllTextAsync(path, JsonSerializer.Serialize(new { success = false, error = error.Message }));
            return 1;
        }
    }
}
