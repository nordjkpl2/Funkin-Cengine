/* // * Friday Night Funkin´ c engine (c) 2026 nordjkpl */
/*
 * Display.c - taxa do monitor (EnumDisplaySettings) e swap interval (wglSwapIntervalEXT).
 * Windows: windows.h. Outras plataformas: devolvem 0 / false e o chamador usa o raylib.
 */
#include "rendering/Display.h"

#if defined(_WIN32) || defined(_WIN64)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

typedef BOOL (WINAPI *PFN_WglSwapIntervalEXT)(int interval);

int Display_RefreshHz(void *hwnd)
{
    HMONITOR monitor = MonitorFromWindow(hwnd != NULL ? (HWND)hwnd : GetActiveWindow(), MONITOR_DEFAULTTONEAREST);
    if (monitor == NULL)
        return 0;

    MONITORINFOEXW info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, (LPMONITORINFO)&info))
        return 0;

    DEVMODEW mode;
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode))
        return 0;

    /* 0 ou 1 = "padrao do hardware": nao e uma taxa real. */
    return mode.dmDisplayFrequency > 1 ? (int)mode.dmDisplayFrequency : 0;
}

bool Display_SetSwapInterval(bool on)
{
    PFN_WglSwapIntervalEXT setInterval = (PFN_WglSwapIntervalEXT)(void *)wglGetProcAddress("wglSwapIntervalEXT");
    if (setInterval == NULL)
        return false;
    return setInterval(on ? 1 : 0) != FALSE;
}

#else

int Display_RefreshHz(void *hwnd)
{
    (void)hwnd;
    return 0;
}

bool Display_SetSwapInterval(bool on)
{
    (void)on;
    return false;
}

#endif
