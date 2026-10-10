using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using EModLoader.Core;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.Wpf;

namespace EModLoader.Desktop;
public sealed class ModuleWindow : Window
{
    private readonly ModuleStore store; private readonly HostConnection host; private readonly InstalledModule module;
    private readonly WebView2 web = new(); private readonly TextBlock status = new() { Margin = new Thickness(14, 10, 14, 10) };
    private readonly DispatcherTimer poll = new() { Interval = TimeSpan.FromMilliseconds(400) };
    private readonly string origin; private bool closed, polling;
    public string Generation => module.Generation;
    internal CoreWebView2? Browser => web.CoreWebView2;
    public ModuleWindow(ModuleStore store, HostConnection host, InstalledModule module)
    {
        Style = (Style)Application.Current.FindResource(typeof(Window));
        this.store = store; this.host = host; this.module = module; origin = "m-" + module.Generation.Replace("-", "") + ".eml.local";
        Title = "E Mod Loader · " + module.Name; Width = 1050; Height = 780; WindowStartupLocation = WindowStartupLocation.CenterOwner;
        var layout = new DockPanel(); DockPanel.SetDock(status, Dock.Top); layout.Children.Add(status); layout.Children.Add(web); Content = layout;
        Loaded += Initialize;
        Closed += (_, _) => { closed = true; poll.Stop(); web.Dispose(); };
        poll.Tick += Poll;
    }
    private bool Owned(string source) => Uri.TryCreate(source, UriKind.Absolute, out var uri) && uri.Scheme == "https" && uri.Host == origin;
    private async void Initialize(object sender, RoutedEventArgs e)
    {
        Loaded -= Initialize;
        try
        {
            status.Text = "正在打开模块界面…";
            var environment = await CoreWebView2Environment.CreateAsync(null, Path.Combine(store.Root, "webview-profile"));
            if (closed) return; await web.EnsureCoreWebView2Async(environment); if (closed) return;
            var core = web.CoreWebView2; core.Settings.AreHostObjectsAllowed = false;
            core.SetVirtualHostNameToFolderMapping(origin, module.Directory, CoreWebView2HostResourceAccessKind.DenyCors);
            core.NavigationStarting += (_, args) => { if (!Owned(args.Uri)) args.Cancel = true; };
            core.NewWindowRequested += (_, args) => args.Handled = true;
            core.WebMessageReceived += Message;
            // Packaged module pages load only resources belonging to their generation.
            core.AddWebResourceRequestedFilter("*", CoreWebView2WebResourceContext.All);
            core.WebResourceRequested += (_, args) => { if (!Owned(args.Request.Uri)) args.Response = core.Environment.CreateWebResourceResponse(null, 403, "Forbidden", ""); };
            await core.AddScriptToExecuteOnDocumentCreatedAsync(WebBridge.Script);
            core.NavigationCompleted += (_, args) => { if (!closed) status.Text = args.IsSuccess ? "模块界面 · 配置保存在 E Mod Loader" : "网页加载失败"; };
            web.Source = new Uri("https://" + origin + "/" + module.Ui); poll.Start();
        }
        catch (Exception error) { if (!closed) status.Text = error.Message; }
    }
    private async void Message(object? sender, CoreWebView2WebMessageReceivedEventArgs e)
    {
        if (closed || !Owned(e.Source) || e.WebMessageAsJson.Length > 256 * 1024) return;
        string requestId = "";
        try
        {
            var envelope = JsonNode.Parse(e.WebMessageAsJson)!.AsObject();
            if (envelope["protocol"]?.GetValue<string>() != WebBridge.Protocol) return;
            requestId = envelope["request_id"]!.GetValue<string>(); if (requestId.Length is 0 or > 128) return;
            var result = await WebBridge.DispatchAsync(store, host, module.Id, envelope["operation"]!.GetValue<string>(), envelope["payload"], requestId);
            Reply(requestId, result);
        }
        catch (Exception error) { if (requestId.Length > 0) Reply(requestId, null, error.Message); }
    }
    private void Reply(string requestId, JsonNode? value, string? error = null)
    { if (!closed) web.CoreWebView2.PostWebMessageAsJson(new JsonObject { ["kind"] = "bridge_reply", ["request_id"] = requestId, ["value"] = value?.DeepClone(), ["error"] = error }.ToJsonString()); }
    private async void Poll(object? sender, EventArgs e)
    {
        if (closed || polling) return; polling = true;
        try
        {
            var state = await host.RequestAsync("status", module.Id);
            if (!closed) status.Text = state["mode"]?.GetValue<string>() == "offline" ? "离线宿主 · 配置保存在 E Mod Loader" : "游戏宿主 · 配置保存在 E Mod Loader";
            var response = await host.RequestAsync("poll", module.Id);
            if (!closed && response["messages"] is JsonArray messages)
                foreach (var message in messages) web.CoreWebView2.PostWebMessageAsJson(new JsonObject { ["kind"] = "runtime_message", ["message"] = message?.DeepClone() }.ToJsonString());
        }
        catch (Exception error) when (error is HttpRequestException or TaskCanceledException or InvalidOperationException) { if (!closed) status.Text = "宿主未连接 · 可继续编辑本地配置"; }
        finally { polling = false; }
    }
}
