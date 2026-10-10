using System.ComponentModel;
using System.Diagnostics;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using EModLoader.Core;
using Microsoft.Win32;

namespace EModLoader.Desktop;
public partial class MainWindow : Window
{
    private readonly ModuleStore store = new();
    private readonly HostConnection host;
    private readonly DispatcherTimer poll = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly Dictionary<string, ModuleWindow> windows = new();
    private JsonNode? runtime;
    private bool polling, closing, closed;
    public MainWindow()
    {
        InitializeComponent(); host = new HostConnection(store);
        host.Log += message => Dispatcher.BeginInvoke(() => AppendLog(message));
        Loaded += async (_, _) => { await Run(async () => { await store.EnsureAsync(); await RefreshRuntime(); }); poll.Start(); };
        poll.Tick += async (_, _) => { if (polling || closing) return; polling = true; try { await RefreshRuntime(); } finally { polling = false; } };
    }
    private void AppendLog(string message)
    { HostLog.AppendText(message + Environment.NewLine); if (HostLog.Text.Length > 64000) HostLog.Text = HostLog.Text[^32000..]; HostLog.ScrollToEnd(); }
    private async Task Run(Func<Task> action)
    { try { await action(); if (!closing) Render(); } catch (Exception error) { StatusLabel.Text = error.Message; AppendLog(error.Message); } }
    private void Render()
    {
        var modules = store.Modules();
        ModuleList.ItemsSource = modules.Select(item => new ModuleCard(item, runtime));
        EmptyLabel.Visibility = modules.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        OfflineButton.Content = host.OwnsOfflineHost ? "停止离线宿主" : "启动离线宿主";
    }
    private async Task RefreshRuntime()
    {
        try { runtime = await host.RequestAsync("status"); }
        catch (HttpRequestException) { runtime = null; }
        catch (TaskCanceledException) { runtime = null; }
        catch (Exception error) { runtime = null; StatusLabel.Text = error.Message; }
        if (closing) return;
        ConnectionLabel.Text = runtime is null ? "未连接" : runtime["mode"]?.GetValue<string>() == "offline" ? "● 离线宿主已连接" : "● 游戏宿主已连接";
        Render();
    }
    private async void Import_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog { Filter = "模块 ZIP|*.zip", Multiselect = true };
        if (picker.ShowDialog(this) != true) return;
        await Run(async () => { foreach (string path in picker.FileNames) { var item = await store.ImportAsync(path); StatusLabel.Text = $"已导入 {item.Name}；更新保留配置，已加载库更新需重启宿主。"; } });
    }
    private async void Offline_Click(object sender, RoutedEventArgs e) => await Run(async () =>
    { if (host.OwnsOfflineHost) await host.StopOfflineAsync(); else await host.StartOfflineAsync(); await RefreshRuntime(); StatusLabel.Text = host.OwnsOfflineHost ? "离线宿主运行中，模块消息在此测试进程处理。" : "离线宿主已停止。"; });
    private async void Refresh_Click(object sender, RoutedEventArgs e) => await Run(RefreshRuntime);
    private async void LaunchGame_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog { Title = "选择 Endfield.exe", Filter = "游戏客户端|Endfield.exe|可执行程序|*.exe" };
        if (picker.ShowDialog(this) != true) return;
        await Run(async () => { await host.StopOfflineAsync(); int pid = await GameLauncher.LaunchAsync(store, picker.FileName); StatusLabel.Text = $"游戏已启动（PID {pid}），等待宿主连接。"; await RefreshRuntime(); });
    }
    private async void Enabled_Click(object sender, RoutedEventArgs e)
    {
        var checkbox = (CheckBox)sender; string id = (string)checkbox.Tag;
        await Run(async () => { await store.SetEnabledAsync(id, checkbox.IsChecked == true); StatusLabel.Text = "启用状态已保存。"; });
    }
    private void Web_Click(object sender, RoutedEventArgs e)
    {
        string id = (string)((Button)sender).Tag;
        try
        {
            var item = store.Find(id);
            if (windows.TryGetValue(id, out var old)) { if (old.Generation == item.Generation) { old.Activate(); return; } old.Close(); }
            var window = new ModuleWindow(store, host, item) { Owner = this }; windows[id] = window;
            window.Closed += (_, _) => { if (windows.GetValueOrDefault(id) == window) windows.Remove(id); }; window.Show();
        }
        catch (Exception error) { StatusLabel.Text = error.Message; }
    }
    private void Configuration_Click(object sender, RoutedEventArgs e)
    {
        string id = (string)((Button)sender).Tag;
        try { new ConfigurationWindow(store, id) { Owner = this }.ShowDialog(); Render(); }
        catch (Exception error) { StatusLabel.Text = error.Message; }
    }
    private async void Logs_Click(object sender, RoutedEventArgs e) => await Run(async () =>
    {
        string id = (string)((Button)sender).Tag; var status = await host.RequestAsync("status", id);
        HostLog.Text = string.Join(Environment.NewLine, status["module"]?["logs"]?.AsArray().Select(n => n!.GetValue<string>()) ?? []);
    });
    private async void MoveUp_Click(object sender, RoutedEventArgs e) => await Run(() => store.MoveAsync((string)((Button)sender).Tag, -1));
    private async void MoveDown_Click(object sender, RoutedEventArgs e) => await Run(() => store.MoveAsync((string)((Button)sender).Tag, 1));
    private async void Remove_Click(object sender, RoutedEventArgs e)
    {
        string id = (string)((Button)sender).Tag;
        if (MessageBox.Show(this, $"从模块库移除 {store.Find(id).Name}？", "移除模块", MessageBoxButton.YesNo) != MessageBoxResult.Yes) return;
        await Run(async () => { if (windows.TryGetValue(id, out var window)) window.Close(); await store.RemoveAsync(id); StatusLabel.Text = "模块已移除；正在使用的安装代次保留至后续清理。"; });
    }
    private static void Open(string target) => Process.Start(new ProcessStartInfo(target) { UseShellExecute = true });
    private void Workshop_Click(object sender, RoutedEventArgs e) => Open("https://146.235.16.65:8443/endfield/");
    private void Repository_Click(object sender, RoutedEventArgs e) => Open("https://github.com/Dr-hydra/Endfield-Mod-Loader");
    private void DataDirectory_Click(object sender, RoutedEventArgs e) { Directory.CreateDirectory(ProductPaths.DataRoot); Open(ProductPaths.DataRoot); }
    protected override async void OnClosing(CancelEventArgs e)
    {
        if (closed) { base.OnClosing(e); return; }
        e.Cancel = true; if (closing) return; closing = true; poll.Stop();
        foreach (var window in windows.Values.ToArray()) window.Close();
        try { await host.DisposeAsync(); } finally { closed = true; Close(); }
    }
    private sealed record ModuleCard
    {
        public string Id { get; } public string Name { get; } public string Subtitle { get; }
        public bool Enabled { get; } public bool Supported { get; } public bool HasUi { get; } public string RuntimeStatus { get; }
        public ModuleCard(InstalledModule item, JsonNode? runtime)
        {
            Id = item.Id; Name = item.Name; Subtitle = $"{item.Author}  ·  {item.Version}  ·  {item.Id}";
            Enabled = item.Enabled; Supported = item.Supported; HasUi = item.Error.Length == 0 && item.Ui.Length > 0;
            var state = runtime?["modules"]?.AsArray().FirstOrDefault(node => node?["id"]?.GetValue<string>() == Id);
            string? status = state?["status"]?.GetValue<string>();
            RuntimeStatus = item.Error.Length > 0 ? item.Error : state?["restart_required"]?.GetValue<bool>() == true ? "需要重启宿主以应用更新" : status switch
            { "ready" => "运行中", "disabled" => "已停用", "ui_only" => "网页模块", "waiting_dependencies" => "等待依赖", "failed" => "加载失败", _ => Enabled ? "已启用 · 等待宿主" : "已停用" };
            if (state?["error"]?.GetValue<string>() is string error && error.Length > 0) RuntimeStatus += " · " + error;
        }
    }
}
