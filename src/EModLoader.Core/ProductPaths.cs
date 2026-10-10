namespace EModLoader.Core;

public static class ProductPaths
{
    public static string DataRoot => Path.GetFullPath(Environment.GetEnvironmentVariable("EML_DATA_ROOT")
        ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "EndfieldModLoader"));
    public static string ModulesRoot => Path.Combine(DataRoot, "modules");
    public static string RuntimeExecutable => Path.Combine(AppContext.BaseDirectory, "runtime", "EModLoader.OfflineHost.exe");
}
