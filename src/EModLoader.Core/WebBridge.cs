using System.Text.Json.Nodes;

namespace EModLoader.Core;

public static class WebBridge
{
    public const string Protocol = "better-endfield.module-ui.v1";
    public const string Script = """
    (() => {
      if (window.top !== window || window.betterEndfield) return;
      let serial = 0; const pending = new Map(), listeners = new Set();
      const session = Math.random().toString(36).slice(2);
      function invoke(operation, payload) {
        const request_id = session + ':' + (++serial);
        return new Promise((resolve, reject) => {
          const timeout = setTimeout(() => { pending.delete(request_id); reject(new Error('Host request timed out')); }, 15000);
          pending.set(request_id, { resolve, reject, timeout });
          const envelope = { protocol: 'better-endfield.module-ui.v1', request_id, operation, payload };
          try {
            if (window.chrome?.webview) window.chrome.webview.postMessage(envelope);
            else window.BetterEndfieldModuleHost.postMessage(JSON.stringify(envelope));
          } catch (error) { clearTimeout(timeout); pending.delete(request_id); reject(error); }
        });
      }
      window.__beModuleDeliver = envelope => {
        if (envelope.kind === 'bridge_reply') {
          const item = pending.get(envelope.request_id); if (!item) return;
          clearTimeout(item.timeout); pending.delete(envelope.request_id);
          envelope.error ? item.reject(new Error(envelope.error)) : item.resolve(envelope.value);
        } else if (envelope.kind === 'runtime_message') {
          for (const listener of listeners) { try { listener(envelope.message); } catch (error) { console.error(error); } }
        }
      };
      if (window.chrome?.webview) window.chrome.webview.addEventListener('message', e => window.__beModuleDeliver(e.data));
      const api = Object.freeze({ readConfig: () => invoke('readConfig'), saveConfig: value => invoke('saveConfig', value),
        send: body => invoke('send', body), status: () => invoke('status'),
        onmessage: listener => { if (typeof listener !== 'function') throw new TypeError('Expected a function'); listeners.add(listener); return () => listeners.delete(listener); } });
      Object.defineProperty(window, 'betterEndfield', { value: api });
      Object.defineProperty(window, 'endfieldModLoader', { value: api });
    })();
    """;
    public static async Task<JsonNode?> DispatchAsync(ModuleStore store, HostConnection host, string id, string operation, JsonNode? payload, string requestId)
    {
        switch (operation)
        {
            case "readConfig": return store.Find(id).State["configuration"]?.DeepClone();
            case "saveConfig":
                var configuration = payload?.AsObject() ?? throw new InvalidDataException("配置必须是 JSON 对象。");
                await store.SaveConfigurationAsync(id, configuration);
                try { await host.RequestAsync("configure", id, configuration); }
                catch (HttpRequestException) { }
                catch (TaskCanceledException) { }
                return new JsonObject { ["saved"] = true };
            case "send": return await host.RequestAsync("send", id, payload, requestId);
            case "status":
                try { return await host.RequestAsync("status", id); }
                catch (HttpRequestException) { return new JsonObject { ["connected"] = false }; }
                catch (TaskCanceledException) { return new JsonObject { ["connected"] = false }; }
            default: throw new InvalidOperationException("不支持的网页操作。");
        }
    }
}
