Add-Type @'
using System;
using System.Runtime.InteropServices;
public class WinEnum {
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint lpdwProcessId);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
    [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }

    public static void Run() {
        EnumWindows((hWnd, lParam) => {
            if (!IsWindowVisible(hWnd)) return true;
            uint pid = 0;
            GetWindowThreadProcessId(hWnd, out pid);
            RECT rc;
            GetClientRect(hWnd, out rc);
            if (rc.Right >= 300 && rc.Bottom >= 200) {
                var sb = new System.Text.StringBuilder(256);
                GetWindowText(hWnd, sb, 256);
                RECT rw;
                GetWindowRect(hWnd, out rw);
                POINT pt = new POINT();
                ClientToScreen(hWnd, ref pt);
                Console.WriteLine("HWND: 0x" + hWnd.ToString("X") + " PID: " + pid + " Client: " + rc.Right + "x" + rc.Bottom + " Origin: (" + pt.X + "," + pt.Y + ") WinRect: [" + rw.Left + "," + rw.Top + "] Title: " + sb.ToString());
            }
            return true;
        }, IntPtr.Zero);
    }
}
'@
[WinEnum]::Run()
