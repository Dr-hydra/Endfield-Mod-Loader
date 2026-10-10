using System.Diagnostics;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json.Nodes;

namespace EModLoader.Core;

public sealed class HostConnection : IAsyncDisposable
{
    private readonly ModuleStore store;
    private Process? process;
    private readonly SemaphoreSlim lifecycle = new(1, 1);
    private readonly HttpClient http = new(new HttpClientHandler { UseProxy = false }) { Timeout = TimeSpan.FromSeconds(3) };
    public HostConnection(ModuleStore store) => this.store = store;
    public event Action<string>? Log;
    public bool OwnsOfflineHost => process is { HasExited: false };
    public async Task<JsonNode> RequestAsync(string operation, string? moduleId = null, JsonNode? payload = null, string? requestId = null)
    {
        if (operation is not ("status" or "poll" or "send" or "configure")) throw new ArgumentException("未知宿主操作。");
        if (moduleId is not null) _ = store.Find(moduleId);
        var index = store.LoadIndex();
        using var request = new HttpRequestMessage(operation == "status" ? HttpMethod.Get : HttpMethod.Post,
            $"http://127.0.0.1:{index["port"]!.GetValue<int>()}/{operation}");
        request.Headers.Authorization = new AuthenticationHeaderValue("Bearer", index["token"]!.GetValue<string>());
        if (operation != "status")
        {
            var body = new JsonObject { ["module_id"] = moduleId };
            if (operation == "send") { body["request_id"] = requestId ?? Guid.NewGuid().ToString(); body["body"] = payload?.DeepClone(); }
            if (operation == "configure") body["configuration"] = payload?.DeepClone();
            request.Content = new StringContent(body.ToJsonString(), Encoding.UTF8, "application/json");
        }
        using var response = await http.SendAsync(request, HttpCompletionOption.ResponseHeadersRead);
        await using var stream = await response.Content.ReadAsStreamAsync(); using var bytes = new MemoryStream();
        byte[] buffer = new byte[8192]; int read;
        while ((read = await stream.ReadAsync(buffer)) > 0)
        { if (bytes.Length + read > 1024 * 1024) throw new InvalidDataException("宿主响应超过上限。"); await bytes.WriteAsync(buffer.AsMemory(0, read)); }
        var result = JsonNode.Parse(bytes.ToArray()) ?? throw new InvalidDataException("宿主响应无效。");
        if (!response.IsSuccessStatusCode) throw new HttpRequestException(result["error"]?.GetValue<string>() ?? "宿主拒绝请求。");
        if (operation == "status")
        {
            if (result["host_product"]?.GetValue<string>() != "Endfield Mod Loader") throw new InvalidDataException("连接到了其他产品的宿主。");
            if (moduleId is not null)
                return new JsonObject { ["connected"] = true, ["mode"] = result["mode"]?.DeepClone(),
                    ["module"] = result["modules"]?.AsArray().FirstOrDefault(m => m?["id"]?.GetValue<string>() == moduleId)?.DeepClone() };
        }
        return result;
    }
    public async Task StartOfflineAsync(string? executable = null)
    {
        await lifecycle.WaitAsync();
        try
        {
            if (OwnsOfflineHost) return;
            await store.EnsureAsync();
            try { _ = await RequestAsync("status"); throw new InvalidOperationException("此模块库已有运行中的宿主，请先关闭它。"); }
            catch (HttpRequestException) { }
            string target = executable ?? ProductPaths.RuntimeExecutable;
            if (!File.Exists(target)) throw new FileNotFoundException("未找到离线宿主，请检查软件包完整性。", target);
            var info = new ProcessStartInfo(target) { UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardInput = true, RedirectStandardOutput = true, RedirectStandardError = true };
            info.ArgumentList.Add("--index"); info.ArgumentList.Add(store.IndexPath);
            process = Process.Start(info) ?? throw new InvalidOperationException("无法启动离线宿主。");
            process.ErrorDataReceived += (_, e) => { if (e.Data is not null) Log?.Invoke(e.Data); }; process.BeginErrorReadLine();
            string? ready;
            try { ready = await process.StandardOutput.ReadLineAsync().WaitAsync(TimeSpan.FromSeconds(15)); }
            catch { await StopOwnedProcessAsync(); throw; }
            if (ready != "ready") { await StopOwnedProcessAsync(); throw new InvalidOperationException("离线宿主未能启动。"); }
            var status = await RequestAsync("status");
            if (status["mode"]?.GetValue<string>() != "offline") throw new InvalidDataException("宿主模式不匹配。");
            Log?.Invoke("离线宿主已启动。");
        }
        finally { lifecycle.Release(); }
    }
    private async Task StopOwnedProcessAsync()
    {
        if (process is null) return;
        var owned = process; process = null;
        try
        {
            if (!owned.HasExited)
            {
                await owned.StandardInput.WriteLineAsync("stop"); await owned.StandardInput.FlushAsync();
                try { await owned.WaitForExitAsync().WaitAsync(TimeSpan.FromSeconds(10)); }
                catch (TimeoutException) { owned.Kill(entireProcessTree: true); await owned.WaitForExitAsync(); }
            }
        }
        finally { owned.Dispose(); }
    }
    public async Task StopOfflineAsync()
    { await lifecycle.WaitAsync(); try { await StopOwnedProcessAsync(); } finally { lifecycle.Release(); } }
    public async ValueTask DisposeAsync() { await StopOfflineAsync(); http.Dispose(); lifecycle.Dispose(); }
}
