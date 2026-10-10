using System.Windows;

namespace EModLoader.Desktop;
public partial class App : Application
{
    private Mutex? singleton;
    protected override void OnStartup(StartupEventArgs e)
    {
        base.OnStartup(e);
        singleton = new Mutex(true, @"Local\EModLoader.Desktop", out bool created);
        if (!created) { MessageBox.Show("E Mod Loader 已在运行。", "Endfield Mod Loader"); Shutdown(); return; }
        DispatcherUnhandledException += (_, args) => { MessageBox.Show(args.Exception.Message, "E Mod Loader"); args.Handled = true; };
        MainWindow = new MainWindow(); MainWindow.Show();
    }
    protected override void OnExit(ExitEventArgs e) { singleton?.Dispose(); base.OnExit(e); }
}
