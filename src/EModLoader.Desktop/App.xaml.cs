using System.Windows;
using EModLoader.Core;

namespace EModLoader.Desktop;
public partial class App : Application
{
    private Mutex? singleton;
    protected override async void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        if (e.Args.Length == 2 && e.Args[0] == "--self-check")
        {
            ShutdownMode = ShutdownMode.OnExplicitShutdown;
            Shutdown(await PackageSelfCheck.RunAsync(e.Args[1])); return;
        }
        singleton = new Mutex(true, @"Local\EModLoader.Desktop", out bool created);
        if (!created) { MessageBox.Show("E Mod Loader 已在运行。", "Endfield Mod Loader"); Shutdown(); return; }
        DispatcherUnhandledException += (_, args) => { MessageBox.Show(args.Exception.Message, "E Mod Loader"); args.Handled = true; };
        try { MainWindow = new MainWindow(); MainWindow.Show(); }
        catch (Exception error) { MessageBox.Show(error.Message, "E Mod Loader 启动失败"); Shutdown(1); }
    }
    protected override void OnExit(ExitEventArgs e) { singleton?.Dispose(); base.OnExit(e); }
}
