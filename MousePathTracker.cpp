// MousePathTracker.cpp
// Minimal UI: shows only distances (Meters, Kilometers, Miles).
// No hotkeys, no buttons, no status bar. Fixed-size, no maximize/resize.
// Minimize-to-tray supported; tray menu offers Restore/Start-Pause/Reset/Exit.
// Saves state to INI every minute and on exit; loads on start.
//
// Programmer: Bob Paydar
//
// © 2025 Bob Paydar. MIT License.

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>
#include <map>
#include <string>
#include <cmath>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Shcore.lib")

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

enum : UINT { WM_TRAYICON = WM_APP + 1, TRAY_ICON_ID = 100, TIMER_UI = 1, TIMER_SAVE = 2 };

struct MonitorMetrics {
    HMONITOR hmon{};
    std::wstring device;
    double pxPerMM_X{ 0.0 };
    double pxPerMM_Y{ 0.0 };
};

// Globals
HINSTANCE g_hInst{};
HWND g_hMain{};
HWND g_hEdit{};
HHOOK g_hook{};
POINT g_lastPt{};
bool g_hasLast{ false };
bool g_running{ true };
bool g_inTray{ false };
HICON g_hIcon{};
double g_totalMM = 0.0;
std::map<HMONITOR, MonitorMetrics> g_monitors;

// Forward decls
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam);
void EnumerateMonitors();
MonitorMetrics GetMetricsAtPoint(POINT pt);
void UpdateUI(HWND);
void ResetCounters();
void MinimizeToTray(HWND hWnd);
void RestoreFromTray(HWND hWnd);
void EnsureTrayIcon(HWND hWnd, bool add);
HMENU BuildTrayMenu();
std::wstring GetIniPath();
void SaveState();
void LoadState();

static std::wstring FormatDouble(double v, int decimals = 3) {
    wchar_t buf[128];
    StringCchPrintfW(buf, 128, L"%.*f", decimals, v);
    return buf;
}

static BOOL CALLBACK MonEnumProc(HMONITOR hMon, HDC, LPRECT, LPARAM) {
    MONITORINFOEXW mi{}; mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(hMon, &mi)) return TRUE;

    HDC hdc = CreateDC(L"DISPLAY", mi.szDevice, NULL, NULL);
    double pxPerMM_X = 0.0, pxPerMM_Y = 0.0;
    if (hdc) {
        int horzRes = GetDeviceCaps(hdc, HORZRES);
        int vertRes = GetDeviceCaps(hdc, VERTRES);
        int horzSizeMM = GetDeviceCaps(hdc, HORZSIZE);
        int vertSizeMM = GetDeviceCaps(hdc, VERTSIZE);
        if (horzRes > 0 && horzSizeMM > 0) pxPerMM_X = (double)horzRes / (double)horzSizeMM;
        if (vertRes > 0 && vertSizeMM > 0) pxPerMM_Y = (double)vertRes / (double)vertSizeMM;
        DeleteDC(hdc);
    }
    MonitorMetrics mm{};
    mm.hmon = hMon;
    mm.device = mi.szDevice;
    mm.pxPerMM_X = pxPerMM_X;
    mm.pxPerMM_Y = pxPerMM_Y;
    g_monitors[hMon] = mm;
    return TRUE;
}

void EnumerateMonitors() {
    g_monitors.clear();
    EnumDisplayMonitors(NULL, NULL, MonEnumProc, 0);
}

MonitorMetrics GetMetricsAtPoint(POINT pt) {
    HMONITOR h = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    auto it = g_monitors.find(h);
    if (it != g_monitors.end()) return it->second;

    MonitorMetrics mm{};
    HDC hdc = GetDC(NULL);
    if (hdc) {
        int horzRes = GetDeviceCaps(hdc, HORZRES);
        int vertRes = GetDeviceCaps(hdc, VERTRES);
        int horzSizeMM = GetDeviceCaps(hdc, HORZSIZE);
        int vertSizeMM = GetDeviceCaps(hdc, VERTSIZE);
        if (horzRes > 0 && horzSizeMM > 0) mm.pxPerMM_X = (double)horzRes / (double)horzSizeMM;
        if (vertRes > 0 && vertSizeMM > 0) mm.pxPerMM_Y = (double)vertRes / (double)vertSizeMM;
        ReleaseDC(NULL, hdc);
    }
    if (mm.pxPerMM_X <= 0.0) mm.pxPerMM_X = 96.0 / 25.4;
    if (mm.pxPerMM_Y <= 0.0) mm.pxPerMM_Y = 96.0 / 25.4;
    return mm;
}

// Hook
LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        MSLLHOOKSTRUCT* p = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (wParam == WM_MOUSEMOVE && g_running) {
            POINT pt = p->pt;
            if (g_hasLast) {
                LONG dx = pt.x - g_lastPt.x;
                LONG dy = pt.y - g_lastPt.y;
                if (dx != 0 || dy != 0) {
                    double pdist = std::sqrt((double)dx * dx + (double)dy * dy);
                    if (pdist >= 1.0) {
                        MonitorMetrics m = GetMetricsAtPoint(pt);
                        double mmx = (m.pxPerMM_X > 0.0) ? ((double)dx / m.pxPerMM_X) : 0.0;
                        double mmy = (m.pxPerMM_Y > 0.0) ? ((double)dy / m.pxPerMM_Y) : 0.0;
                        double mm = std::sqrt(mmx * mmx + mmy * mmy);
                        g_totalMM += mm;
                    }
                }
            }
            g_lastPt = pt;
            g_hasLast = true;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}

// UI
void UpdateUI(HWND hWnd) {
    double total_m = g_totalMM / 1000.0;
    double total_km = total_m / 1000.0;
    double total_mi = total_m / 1609.344;

    std::wstring text =
        L"Mouse Path Distance (global):\r\n"
        L"  • Meters:     " + FormatDouble(total_m, 4) + L" m\r\n" +
        L"  • Kilometers: " + FormatDouble(total_km, 6) + L" km\r\n" +
        L"  • Miles:      " + FormatDouble(total_mi, 6) + L" mi\r\n";

    SetWindowTextW(hWnd, L"Mouse Path Tracker — Bob Paydar");
    SetWindowTextW(g_hEdit, text.c_str());
}

// Tray
void EnsureTrayIcon(HWND hWnd, bool add) {
    NOTIFYICONDATA nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hWnd;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    if (!g_hIcon) g_hIcon = LoadIcon(NULL, IDI_APPLICATION);
    nid.hIcon = g_hIcon;
    StringCchCopyW(nid.szTip, ARRAYSIZE(nid.szTip), L"Mouse Path Tracker — Bob Paydar");
    if (add) {
        Shell_NotifyIconW(NIM_ADD, &nid);
#ifdef NOTIFYICON_VERSION_4
        nid.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &nid);
#endif
    }
    else {
        Shell_NotifyIconW(NIM_DELETE, &nid);
    }
}

void MinimizeToTray(HWND hWnd) {
    if (g_inTray) return;
    EnsureTrayIcon(hWnd, true);
    ShowWindow(hWnd, SW_HIDE);
    g_inTray = true;
}

void RestoreFromTray(HWND hWnd) {
    if (!g_inTray) return;
    EnsureTrayIcon(hWnd, false);
    ShowWindow(hWnd, SW_SHOWNORMAL);
    SetForegroundWindow(hWnd);
    g_inTray = false;
}

HMENU BuildTrayMenu() {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return NULL;
    AppendMenuW(hMenu, MF_STRING, 4001, L"&Restore");
    AppendMenuW(hMenu, MF_STRING, 4002, g_running ? L"&Pause" : L"&Start");
    AppendMenuW(hMenu, MF_STRING, 4003, L"&Reset");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, 4004, L"E&xit");
    return hMenu;
}

// INI
std::wstring GetIniPath() {
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::wstring s = exePath;
    size_t pos = s.find_last_of(L"\\/");
    std::wstring dir = (pos == std::wstring::npos) ? L"" : s.substr(0, pos + 1);
    size_t dot = s.find_last_of(L".");
    std::wstring base = (dot == std::wstring::npos) ? s : s.substr(0, dot);
    size_t bslash = base.find_last_of(L"\\/");
    std::wstring fname = (bslash == std::wstring::npos) ? base : base.substr(bslash + 1);
    return dir + fname + L".ini";
}

void SaveState() {
    std::wstring ini = GetIniPath();
    wchar_t buf[64];
    StringCchPrintfW(buf, 64, L"%.8f", g_totalMM);
    WritePrivateProfileStringW(L"MousePathTracker", L"TotalMM", buf, ini.c_str());
    WritePrivateProfileStringW(L"MousePathTracker", L"Running", g_running ? L"1" : L"0", ini.c_str());
}

void LoadState() {
    std::wstring ini = GetIniPath();
    wchar_t buf[128];
    GetPrivateProfileStringW(L"MousePathTracker", L"TotalMM", L"0", buf, 128, ini.c_str());
    g_totalMM = _wtof(buf);
    GetPrivateProfileStringW(L"MousePathTracker", L"Running", L"1", buf, 128, ini.c_str());
    g_running = (buf[0] != L'0');
}

// Window creation
static void CreateChildControls(HWND hWnd) {
    HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
    g_hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL,
        8, 8, 400, 200, hWnd, (HMENU)1001, g_hInst, NULL);
    SendMessageW(g_hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
}

static void ResizeClient(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    int padding = 8;
    MoveWindow(g_hEdit, padding, padding, rc.right - 2 * padding, rc.bottom - 2 * padding, TRUE);
}

// WinMain
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_hInst = hInstance;

    WNDCLASSEXW wcex{};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MousePathTrackerWndClass";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassExW(&wcex)) return 0;

    // Fixed window: caption + system menu, no resize, no maximize
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    g_hMain = CreateWindowExW(0, wcex.lpszClassName, L"Mouse Path Tracker — Bob Paydar",
        style, CW_USEDEFAULT, 0, 520, 260, NULL, NULL, hInstance, NULL);
    if (!g_hMain) return 0;

    ShowWindow(g_hMain, nCmdShow);
    UpdateWindow(g_hMain);

    EnumerateMonitors();
    LoadState();
    g_hook = SetWindowsHookExW(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandleW(NULL), 0);

    SetTimer(g_hMain, TIMER_UI, 200, NULL);
    SetTimer(g_hMain, TIMER_SAVE, 60 * 1000, NULL);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_hook) UnhookWindowsHookEx(g_hook);
    return (int)msg.wParam;
}

// WndProc
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        CreateChildControls(hWnd);
        ResizeClient(hWnd);
        UpdateUI(hWnd);
        break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            MinimizeToTray(hWnd);
            return 0;
        }
        else {
            ResizeClient(hWnd);
        }
        break;
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_MINIMIZE) {
            MinimizeToTray(hWnd);
            return 0;
        }
        // No custom handling for maximize; window doesn't have maximize box
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    case WM_DPICHANGED:
    case WM_DISPLAYCHANGE:
        EnumerateMonitors();
        break;
    case WM_TIMER:
        if (wParam == TIMER_UI) UpdateUI(hWnd);
        else if (wParam == TIMER_SAVE) SaveState();
        break;
    case WM_TRAYICON:
        if (LOWORD(lParam) == WM_LBUTTONUP || LOWORD(lParam) == WM_LBUTTONDBLCLK) {
            RestoreFromTray(hWnd);
        }
        else if (LOWORD(lParam) == WM_RBUTTONUP) {
            POINT pt; GetCursorPos(&pt);
            HMENU hMenu = BuildTrayMenu();
            if (hMenu) {
                SetForegroundWindow(hWnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);
                switch (cmd) {
                case 4001: RestoreFromTray(hWnd); break;
                case 4002: g_running = !g_running; break;
                case 4003: ResetCounters(); break;
                case 4004: SendMessageW(hWnd, WM_CLOSE, 0, 0); break;
                }
                UpdateUI(hWnd);
            }
        }
        break;
    case WM_CLOSE:
        SaveState(); // ensure INI is written before closing
        DestroyWindow(hWnd);
        break;
    case WM_DESTROY:
        KillTimer(hWnd, TIMER_UI);
        KillTimer(hWnd, TIMER_SAVE);
        if (g_inTray) EnsureTrayIcon(hWnd, false);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

// Helpers
void ResetCounters() { g_totalMM = 0.0; g_hasLast = false; }

