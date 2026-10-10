using System.Security.Cryptography;
using System.Text.Json;

namespace EModLoader.Core;

public static class ProxyInstallation
{
    public static bool IsOwned(string game)
    {
        try
        {
            string directory = Path.GetDirectoryName(Path.GetFullPath(game))!;
            string file = Path.Combine(directory, "xinput1_4.dll"), receipt = Path.Combine(directory, "eml-xinput.json");
            if (!File.Exists(file) || !File.Exists(receipt) || new FileInfo(receipt).Length > 16384 || new FileInfo(file).Length > 64 * 1024 * 1024) return false;
            using var json = JsonDocument.Parse(File.ReadAllText(receipt));
            if (json.RootElement.GetProperty("product").GetString() != "EndfieldModLoader" || json.RootElement.GetProperty("schema").GetInt32() != 1) return false;
            using var stream = File.OpenRead(file);
            string digest = Convert.ToHexString(SHA256.HashData(stream));
            return digest.Equals(json.RootElement.GetProperty("proxy_sha256").GetString(), StringComparison.OrdinalIgnoreCase);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException or InvalidOperationException or KeyNotFoundException or ArgumentException) { return false; }
    }
}
