using System.Text.Json;
using System.Text.Json.Nodes;
using System.Windows;
using System.Windows.Controls;
using EModLoader.Core;

namespace EModLoader.Desktop;
public sealed class ConfigurationWindow : Window
{
    public ConfigurationWindow(ModuleStore store, string id)
    {
        Style = (Style)Application.Current.FindResource(typeof(Window));
        var item = store.Find(id); Title = item.Name + " · 配置"; Width = 650; Height = 540; WindowStartupLocation = WindowStartupLocation.CenterOwner;
        var layout = new DockPanel { Margin = new Thickness(20) };
        var bar = new StackPanel { Orientation = Orientation.Horizontal, Margin = new Thickness(0, 14, 0, 0) };
        var save = new Button { Content = "保存配置" }; var status = new TextBlock { VerticalAlignment = VerticalAlignment.Center, TextWrapping = TextWrapping.Wrap };
        bar.Children.Add(save); bar.Children.Add(status); DockPanel.SetDock(bar, Dock.Bottom); layout.Children.Add(bar);
        var editor = new TextBox { AcceptsReturn = true, AcceptsTab = true, VerticalScrollBarVisibility = ScrollBarVisibility.Auto,
            FontFamily = new System.Windows.Media.FontFamily("Consolas"), Text = item.State["configuration"]!.ToJsonString(new JsonSerializerOptions { WriteIndented = true }) };
        layout.Children.Add(editor); Content = layout;
        save.Click += async (_, _) => { try { var config = JsonNode.Parse(editor.Text)?.AsObject() ?? throw new InvalidDataException("配置必须是 JSON 对象。"); await store.SaveConfigurationAsync(id, config); status.Text = "已保存"; } catch (Exception error) { status.Text = error.Message; } };
    }
}
