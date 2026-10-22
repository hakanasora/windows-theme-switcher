using System;
using System.Runtime.InteropServices;
using Microsoft.Win32;

// Recovered from the original ThemeSwitcher.exe. Comments and formatting are new.
class ThemeSwitcher
{
    private static readonly IntPtr HWND_BROADCAST = new IntPtr(0xffff);
    private const uint WM_SETTINGCHANGE = 0x001a;

    [DllImport("user32.dll", CharSet = CharSet.Auto, SetLastError = true)]
    private static extern IntPtr SendMessageTimeout(
        IntPtr hWnd,
        uint Msg,
        UIntPtr wParam,
        string lParam,
        uint fuFlags,
        uint uTimeout,
        out UIntPtr lpdwResult);

    private static void Main(string[] args)
    {
        if (args.Length == 0)
            return;

        string mode = args[0].ToLower();
        int themeValue;

        if (mode == "dark")
        {
            themeValue = 0;
        }
        else if (mode == "light")
        {
            themeValue = 1;
        }
        else
        {
            return;
        }

        try
        {
            using (RegistryKey key = Registry.CurrentUser.OpenSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\Themes\Personalize", true))
            {
                if (key != null)
                {
                    key.SetValue("AppsUseLightTheme", themeValue, RegistryValueKind.DWord);
                    key.SetValue("SystemUsesLightTheme", themeValue, RegistryValueKind.DWord);
                }
            }

            UIntPtr result;
            SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE, UIntPtr.Zero,
                "ImmersiveColorSet", 0x0002, 5000, out result);
        }
        catch (Exception)
        {
            // Preserve the original silent failure behavior.
        }
    }
}
