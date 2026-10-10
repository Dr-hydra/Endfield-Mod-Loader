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
        if (args.Length != 3) throw new ArgumentException("Supply original ZIP, OfflineHost.exe and output directory.");
        string output = Path.GetFullPath(args[2]); Directory.CreateDirectory(output);
        string data = Path.Combine(output, "data"); Environment.SetEnvironmentVariable("EML_DATA_ROOT", data);
        var store = new ModuleStore(); store.EnsureAsync().GetAwaiter().GetResult();
        var app = new EModLoader.Desktop.App(); app.InitializeComponent();
        app.Startup += async (_, _) =>
        {
            await Task.Delay(100);
            int result = 1; await using var host = new HostConnection(store);
            try
            {
                var item = await store.ImportAsync(args[0]); await store.SetEnabledAsync(item.Id, true); await host.StartOfflineAsync(args[1]);
                var window = new ModuleWindow(store, host, store.Find(item.Id)) { Owner = app.MainWindow }; window.Show();
                for (int i = 0; i < 100 && (window.Browser is null || !window.Browser.Source.StartsWith("https://m-")); i++) await Task.Delay(100);
                if (window.Browser is null) throw new Exception("WebView2 was not initialized.");
                for (int i = 0; i < 100; i++)
                {
                    string ready = await window.Browser.ExecuteScriptAsync("Boolean(window.betterEndfield && document.getElementById('send'))");
                    if (ready == "true") break;
                    if (i == 99) throw new Exception("Published module page or legacy bridge did not load.");
                    await Task.Delay(100);
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
