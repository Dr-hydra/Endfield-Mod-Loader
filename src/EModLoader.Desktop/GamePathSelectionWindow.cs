using System.Windows;
using System.Windows.Controls;

namespace EModLoader.Desktop;

internal sealed class GamePathSelectionWindow : Window
{
    public string? SelectedPath { get; private set; }
    public GamePathSelectionWindow(IReadOnlyList<string> paths)
    {
        Style = (Style)Application.Current.FindResource(typeof(Window));
        Title = "选择游戏安装"; Width = 800; Height = 350; WindowStartupLocation = WindowStartupLocation.CenterOwner;
        var layout = new DockPanel { Margin = new Thickness(20) };
        var hint = new TextBlock { Text = "找到多个安装，请选择要启动的游戏。", Margin = new Thickness(0, 0, 0, 15) };
        DockPanel.SetDock(hint, Dock.Top); layout.Children.Add(hint);
        var list = new ListBox { ItemsSource = paths, Background = System.Windows.Media.Brushes.Transparent,
            Foreground = System.Windows.Media.Brushes.White, Padding = new Thickness(8) };
        var button = new Button { Content = "使用此路径", IsEnabled = false, HorizontalAlignment = HorizontalAlignment.Right, Margin = new Thickness(0, 15, 0, 0) };
        DockPanel.SetDock(button, Dock.Bottom); layout.Children.Add(button); layout.Children.Add(list); Content = layout;
        list.SelectionChanged += (_, _) => button.IsEnabled = list.SelectedItem is string;
        void Accept() { if (list.SelectedItem is string path) { SelectedPath = path; DialogResult = true; } }
        button.Click += (_, _) => Accept(); list.MouseDoubleClick += (_, _) => Accept();
    }
}
