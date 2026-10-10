using System.IO;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using EModLoader.Core;
using EModLoader.Desktop;

internal static class Program
{
    [STAThread]
    public static int Main(string[] args)
    {
        if (args.Length is not (3 or 4)) throw new ArgumentException("Supply ZIP, OfflineHost.exe, output directory and optional first-person mode.");
        bool firstPerson = args.Length == 4 && args[3] == "first-person";
        string output = Path.GetFullPath(args[2]); Directory.CreateDirectory(output);
        string data = Path.Combine(output, "data"); Environment.SetEnvironmentVariable("EML_DATA_ROOT", data);
        var store = new ModuleStore(); store.EnsureAsync().GetAwaiter().GetResult();
        string gameDirectory = Path.Combine(output, "saved game with 空格"); Directory.CreateDirectory(gameDirectory);
        string gamePath = Path.Combine(gameDirectory, "Endfield.exe"); File.WriteAllBytes(gamePath, [0]);
        new GamePathSettings().SaveAsync(gamePath).GetAwaiter().GetResult();
        string testRuntime = Path.Combine(AppContext.BaseDirectory, "runtime"); Directory.CreateDirectory(testRuntime);
        foreach (string name in new[] { "EModLoader.Launcher.exe", "EModLoader.Host.dll", "EModLoader.XInputProxy.dll" })
            File.Copy(Path.Combine(Path.GetDirectoryName(Path.GetFullPath(args[1]))!, name), Path.Combine(testRuntime, name), true);
        var app = new EModLoader.Desktop.App(); app.InitializeComponent();
        app.Startup += async (_, _) =>
        {
            await Task.Delay(100);
            int result = 1; await using var host = new HostConnection(store);
            try
            {
                var desktop = (EModLoader.Desktop.MainWindow)app.MainWindow;
                var pathLabel = (System.Windows.Controls.TextBox)desktop.FindName("GamePathLabel");
                for (int i = 0; i < 100 && pathLabel.Text != gamePath; i++) await Task.Delay(100);
                if (pathLabel.Text != gamePath) throw new Exception("Desktop did not restore the saved game path.");
                await GameLauncher.DeployProxyAsync(store, gamePath, true);
                if (!ProxyInstallation.IsOwned(gamePath)) throw new Exception("Actual desktop runtime did not install the owned proxy through its result pipe.");
                await GameLauncher.DeployProxyAsync(store, gamePath, false);
                if (File.Exists(Path.Combine(gameDirectory, "xinput1_4.dll"))) throw new Exception("Actual desktop runtime did not uninstall its proxy.");
                var item = await store.ImportAsync(args[0]); await store.SetEnabledAsync(item.Id, true); await host.StartOfflineAsync(args[1]);
                var window = new ModuleWindow(store, host, store.Find(item.Id)) { Owner = app.MainWindow };
                if (firstPerson) { window.Width = 1050; window.Height = 1000; }
                window.Show();
                for (int i = 0; i < 100 && (window.Browser is null || !window.Browser.Source.StartsWith("https://m-")); i++) await Task.Delay(100);
                if (window.Browser is null) throw new Exception("WebView2 was not initialized.");
                for (int i = 0; i < 100; i++)
                {
                    string ready = await window.Browser.ExecuteScriptAsync(firstPerson
                        ? "Boolean(window.betterEndfield && document.getElementById('toggle') && document.getElementById('fov').value)"
                        : "Boolean(window.betterEndfield && document.getElementById('send'))");
                    if (ready == "true") break;
                    if (i == 99) throw new Exception("Published module page or legacy bridge did not load.");
                    await Task.Delay(100);
                }
                if (firstPerson)
                {
                    await window.Browser.ExecuteScriptAsync("""
                    window.__emlProof = {};
                    window.betterEndfield.onmessage(message => {
                      if (message.kind === 'reply' && message.body?.phase) window.__emlProof.reply = message;
                    });
                    (async () => {
                      const config = await window.betterEndfield.readConfig();
                      window.__emlProof.defaults = config;
                      document.getElementById('fov').value = '88';
                      document.getElementById('settings').requestSubmit();
                      window.__emlProof.status = await window.betterEndfield.status();
                    })().catch(error => window.__emlProof.error = error.message);
                    """);
                    JsonNode? firstPersonProof = null;
                    for (int i = 0; i < 100; i++)
                    {
                        firstPersonProof = JsonNode.Parse(await window.Browser.ExecuteScriptAsync("window.__emlProof"));
                        if (firstPersonProof?["error"] is not null) throw new Exception(firstPersonProof["error"]!.GetValue<string>());
                        if (firstPersonProof?["reply"]?["body"]?["phase"]?.GetValue<string>() == "waiting_runtime" &&
                            store.Find(item.Id).State["configuration"]?["fov"]?.GetValue<int>() == 88) break;
                        if (i == 99) throw new Exception("First-person UI configuration or native offline reply did not complete.");
                        await Task.Delay(100);
                    }
                    if (firstPersonProof?["defaults"]?["active"]?.GetValue<bool>() != false ||
                        firstPersonProof?["status"]?["mode"]?.GetValue<string>() != "offline") throw new Exception("First-person defaults/offline status differed.");
                    if (await window.Browser.ExecuteScriptAsync("document.getElementById('toggle').disabled") != "true") throw new Exception("Offline page incorrectly allows game controls.");
                    await window.Browser.ExecuteScriptAsync("""
                    (async () => {
                      const config = await window.betterEndfield.readConfig();
                      window.__emlProof.saved = config;
                      await window.betterEndfield.send({action:'toggle'});
                    })().catch(error => window.__emlProof.error = error.message);
                    """);
                    for (int i = 0; i < 100; i++)
                    {
                        firstPersonProof = JsonNode.Parse(await window.Browser.ExecuteScriptAsync("window.__emlProof"));
                        if (firstPersonProof?["reply"]?["result"]?.GetValue<int>() == 2) break;
                        if (i == 99) throw new Exception("Native module did not reject offline toggle.");
                        await Task.Delay(100);
                    }
                    if (firstPersonProof?["saved"]?["fov"]?.GetValue<int>() != 88) throw new Exception("First-person form did not persist configuration.");
                    File.WriteAllText(Path.Combine(output, "web-proof.json"), firstPersonProof!.ToJsonString());
                    using (var stream = File.Create(Path.Combine(output, "module-page.png"))) await window.Browser.CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, stream);
                    desktop.UpdateLayout();
                    var desktopBitmap = new RenderTargetBitmap((int)desktop.ActualWidth, (int)desktop.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                    desktopBitmap.Render(desktop); var desktopEncoder = new PngBitmapEncoder(); desktopEncoder.Frames.Add(BitmapFrame.Create(desktopBitmap));
                    using (var stream = File.Create(Path.Combine(output, "desktop.png"))) desktopEncoder.Save(stream);
                    window.Close();
                    // Verify the real resident DLL can be disabled and initialized again.
                    await store.SetEnabledAsync(item.Id, false);
                    for (int i = 0; i < 50; i++)
                    {
                        var status = await host.RequestAsync("status", item.Id);
                        if (status["module"]?["status"]?.GetValue<string>() == "disabled") break;
                        if (i == 49) throw new Exception("First-person DLL did not disable.");
                        await Task.Delay(100);
                    }
                    await store.SetEnabledAsync(item.Id, true);
                    for (int i = 0; i < 50; i++)
                    {
                        var status = await host.RequestAsync("status", item.Id);
                        if (status["module"]?["status"]?.GetValue<string>() == "ready") break;
                        if (i == 49) throw new Exception("First-person DLL did not reinitialize.");
                        await Task.Delay(100);
                    }
                    Console.WriteLine("PASS: desktop restored the saved path; actual proxy install/uninstall IPC passed; first-person ZIP/DLL/page/config/offline/lifecycle passed. No game or interactive UAC was used.");
                    result = 0;
                    return;
                }
                await window.Browser.ExecuteScriptAsync("""
                window.__emlProof = {};
                window.betterEndfield.onmessage(message => {
                  if (message.kind === 'reply') window.__emlProof.reply = message;
                  if (message.kind === 'event') window.__emlProof.event = message;
                });
                (async () => {
                  const config = await window.betterEndfield.readConfig();
                  await window.betterEndfield.saveConfig({ ...config, label: 'actual WebView2 proof' });
                  window.__emlProof.status = await window.betterEndfield.status();
                  window.__emlProof.accepted = await window.betterEndfield.send({ from: 'published web page' });
                })().catch(error => window.__emlProof.error = error.message);
                """);
                JsonNode? proof = null;
                for (int i = 0; i < 100; i++)
                {
                    proof = JsonNode.Parse(await window.Browser.ExecuteScriptAsync("window.__emlProof"));
                    if (proof?["error"] is not null) throw new Exception(proof["error"]!.GetValue<string>());
                    if (proof?["reply"] is not null && proof["event"] is not null) break;
                    await Task.Delay(100);
                }
                if (proof?["reply"]?["body"]?["configuration"]?["label"]?.GetValue<string>() != "actual WebView2 proof") throw new Exception("Legacy page config did not reach the published DLL.");
                if (proof?["reply"]?["body"]?["echo"]?["from"]?.GetValue<string>() != "published web page") throw new Exception("Actual page reply was lost.");
                if (proof?["status"]?["mode"]?.GetValue<string>() != "offline") throw new Exception("Actual page status lost the offline mode.");
                File.WriteAllText(Path.Combine(output, "web-proof.json"), proof!.ToJsonString());
                using (var stream = File.Create(Path.Combine(output, "module-page.png"))) await window.Browser.CapturePreviewAsync(Microsoft.Web.WebView2.Core.CoreWebView2CapturePreviewImageFormat.Png, stream);
                app.MainWindow.UpdateLayout();
                var bitmap = new RenderTargetBitmap((int)app.MainWindow.ActualWidth, (int)app.MainWindow.ActualHeight, 96, 96, PixelFormats.Pbgra32);
                bitmap.Render(app.MainWindow); var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap));
                using (var stream = File.Create(Path.Combine(output, "desktop.png"))) encoder.Save(stream);
                window.Close(); Console.WriteLine("PASS: actual WebView2 opened the original page; legacy config/status/send/reply/event worked with the original DLL."); result = 0;
            }
            catch (Exception error) { Console.Error.WriteLine(error); }
            finally { await host.StopOfflineAsync(); app.Shutdown(result); }
        };
        return app.Run();
    }
}
