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
    private readonly GamePathSettings gameSettings = new();
    private readonly DispatcherTimer poll = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly Dictionary<string, ModuleWindow> windows = new();
    private JsonNode? runtime;
    private bool polling, closing, closed, findingGame, launchingGame;
    public MainWindow()
    {
        InitializeComponent(); host = new HostConnection(store);
        AllowXInputConflict.IsChecked = gameSettings.LoadAllowXInputConflict();
        host.Log += message => Dispatcher.BeginInvoke(() => AppendLog(message));
        Loaded += async (_, _) => { await Run(async () => { await store.EnsureAsync(); await RefreshRuntime(); await FindGamePathAsync(false); }); poll.Start(); };
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
        if (launchingGame || findingGame) return;
        launchingGame = true; LaunchGameButton.IsEnabled = false;
        AllowXInputConflict.IsEnabled = false;
        DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = InstallProxyButton.IsEnabled = RemoveProxyButton.IsEnabled = false;
        await Run(async () =>
        {
            try
            {
                if (GamePathDiscovery.Resolve(GamePathLabel.Text) is null)
                {
                    await FindGamePathAsync(true);
                    if (GamePathDiscovery.Resolve(GamePathLabel.Text) is null && !await SelectGamePathAsync()) return;
                }
                await host.StopOfflineAsync();
                int pid = await GameLauncher.LaunchAsync(store, GamePathLabel.Text, message => { StatusLabel.Text = message; AppendLog(message); },
                    allowXInputConflict: AllowXInputConflict.IsChecked == true);
                StatusLabel.Text = $"游戏已启动（PID {pid}），等待宿主连接。"; await RefreshRuntime();
            }
            finally { launchingGame = false; AllowXInputConflict.IsEnabled = true; LaunchGameButton.IsEnabled = DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = InstallProxyButton.IsEnabled = true; RefreshProxyState(); }
        });
    }
    private async Task SaveGamePathAsync(string path)
    {
        await gameSettings.SaveAsync(path); GamePathLabel.Text = GamePathDiscovery.Resolve(path)!;
        GamePathHint.Text = "路径已保存，下次启动自动使用。";
        RefreshProxyState();
    }
    private async Task FindGamePathAsync(bool chooseMultiple)
    {
        if (findingGame) return;
        string saved = gameSettings.Load();
        if (!chooseMultiple && GamePathDiscovery.Resolve(saved) is string cached)
        { GamePathLabel.Text = cached; GamePathHint.Text = "已读取保存的游戏路径。"; RefreshProxyState(); return; }
        findingGame = true; DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = false;
        GamePathHint.Text = "正在查找游戏…";
        try
        {
            var found = await Task.Run(() => GamePathDiscovery.Discover(saved));
            if (closing) return;
            if (found.Count == 1) await SaveGamePathAsync(found[0]);
            else if (found.Count > 1)
            {
                GamePathHint.Text = $"找到 {found.Count} 个安装，请选择游戏路径。";
                if (chooseMultiple)
                {
                    var picker = new GamePathSelectionWindow(found) { Owner = this };
                    if (picker.ShowDialog() == true) await SaveGamePathAsync(picker.SelectedPath!);
                }
            }
            else
            {
                GamePathLabel.Text = "";
                GamePathHint.Text = saved.Length > 0 ? "保存的路径已失效，请重新选择 Endfield.exe。" : "未找到游戏，请点击“选择…”指定 Endfield.exe。";
                RefreshProxyState();
            }
        }
        finally { findingGame = false; DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = true; }
    }
    private async Task<bool> SelectGamePathAsync()
    {
        var picker = new OpenFileDialog { Title = "选择 Endfield.exe", Filter = "游戏客户端|Endfield.exe", CheckFileExists = true };
        string saved = gameSettings.Load();
        if (Path.GetDirectoryName(saved) is string directory && Directory.Exists(directory)) picker.InitialDirectory = directory;
        if (picker.ShowDialog(this) != true) return false;
        await SaveGamePathAsync(picker.FileName); return true;
    }
    private async void DetectGame_Click(object sender, RoutedEventArgs e) => await Run(() => FindGamePathAsync(true));
    private async void SelectGame_Click(object sender, RoutedEventArgs e) => await Run(async () => { await SelectGamePathAsync(); });
    private void RefreshProxyState()
    {
        bool installed = GamePathLabel.Text.Length > 0 && ProxyInstallation.IsOwned(GamePathLabel.Text);
        bool foreign = GamePathLabel.Text.Length > 0 && !installed && File.Exists(Path.Combine(Path.GetDirectoryName(GamePathLabel.Text)!, "xinput1_4.dll"));
        bool coexist = AllowXInputConflict.IsChecked == true;
        ProxyStateLabel.Text = installed ? "已安装 · 官方启动器可加载 EML" : foreign ?
            coexist ? "保留现有 XInput · 启动游戏时额外加载 EML" : "已有其他或被修改的 XInput DLL，请先处理冲突" : "未安装 · 可使用注入器启动";
        InstallProxyButton.IsEnabled = !foreign;
        RemoveProxyButton.IsEnabled = installed;
    }
    private async void Compatibility_Click(object sender, RoutedEventArgs e)
    {
        bool enabled = AllowXInputConflict.IsChecked == true;
        AllowXInputConflict.IsEnabled = false;
        try
        {
            await gameSettings.SaveAllowXInputConflictAsync(enabled);
            StatusLabel.Text = enabled ? "已保存：保留其他 XInput DLL，启动时额外加载 EML。" : "已保存：遇到其他 XInput DLL 时停止启动。";
        }
        catch (Exception error) { AllowXInputConflict.IsChecked = gameSettings.LoadAllowXInputConflict(); StatusLabel.Text = "启动设置保存失败：" + error.Message; }
        finally { AllowXInputConflict.IsEnabled = true; RefreshProxyState(); }
    }
    private async Task ChangeProxyAsync(bool install)
    {
        if (launchingGame || findingGame) return;
        await Run(async () =>
        {
            if (GamePathDiscovery.Resolve(GamePathLabel.Text) is null && !await SelectGamePathAsync()) return;
            launchingGame = true; LaunchGameButton.IsEnabled = InstallProxyButton.IsEnabled = RemoveProxyButton.IsEnabled = DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = false;
            try
            {
                await GameLauncher.DeployProxyAsync(store, GamePathLabel.Text, install, message => { StatusLabel.Text = message; AppendLog(message); });
                StatusLabel.Text = install ? "DLL 自启动已安装，可从官方启动器启动游戏。移动 EML 目录后请重新安装自启动。" : "EML 的 DLL 自启动已卸载。";
            }
            finally { launchingGame = false; LaunchGameButton.IsEnabled = InstallProxyButton.IsEnabled = DetectGameButton.IsEnabled = SelectGameButton.IsEnabled = true; RefreshProxyState(); }
        });
    }
    private async void InstallProxy_Click(object sender, RoutedEventArgs e) => await ChangeProxyAsync(true);
    private async void RemoveProxy_Click(object sender, RoutedEventArgs e) => await ChangeProxyAsync(false);
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
