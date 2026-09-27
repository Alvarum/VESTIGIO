using System.Windows;

namespace RetroForge.Studio;

public partial class OnboardingWindow : Window
{
    public OnboardingWindow() => InitializeComponent();
    private void Close_Click(object sender, RoutedEventArgs e) => Close();
}
