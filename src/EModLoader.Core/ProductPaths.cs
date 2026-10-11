namespace EModLoader.Core;

public static class ProductPaths
{
    public static string DataRoot => Path.GetFullPath(Environment.GetEnvironmentVariable("EML_DATA_ROOT")
        ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EndfieldModLoader"));
    public static string ModulesRoot => Path.Combine(DataRoot, "modules");
    private static string? runtimeDirectory;
    public static string RuntimeDirectory => runtimeDirectory ?? Path.Combine(AppContext.BaseDirectory, "runtime");
    public static string RuntimeExecutable => Path.Combine(RuntimeDirectory, "EModLoader.OfflineHost.exe");
    public static void UseRuntimeDirectory(string directory) => runtimeDirectory = Path.GetFullPath(directory);
}
