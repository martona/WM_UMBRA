// SPDX-License-Identifier: MIT
//
// umbra-payload.dll — injected dark-theming payload (umbra-inject harness).
//
// umbra-inject.exe installs ONE global WH_CBT hook whose proc lives here, so this DLL
// maps into target processes as early as their first window — before WM_NCCREATE.
// On the allowlist (regedit) we, once per process, init umbra + install the process-wide
// GetSysColor/uxtheme inline hooks (the proven ProcessColorHook/ThemeColorHook, shared
// from sample-hook), and, once per thread, self-install WH_CALLWNDPROC[RET] on the
// current thread so umbra themes every window at creation (prepDarkModeForNewWindow on
// WM_NCCREATE, applyDarkToNewWindow on WM_CREATE).
//
// HCBT_CREATEWND fires before WM_NCCREATE, so the window that triggered our per-thread
// install is itself caught — even regedit's frame themes through the normal path. And the
// CBT hook's per-thread firing IS the thread coverage, so there is NO CreateThread detour
// and NO child-walk: this is sample-hook's hook layer, productized for injection.
//
// Temporary in-tree sample; removed before this branch merges to umbra's main.

#include <windows.h>
#include <dwmapi.h>
#include <strsafe.h>

#include <umbra.h>
#include "hook.h"   // setProcessWideColorHook / setProcessWideThemeColorHook (sample-hook)

namespace
{
    bool          g_isTarget = false;                 // are we inside a whitelisted process?
    HMODULE       g_self     = nullptr;               // this DLL (for SetWindowsHookEx hMod)
    INIT_ONCE     g_initOnce = INIT_ONCE_STATIC_INIT; // once-per-process umbra + inline-hook init
    volatile LONG g_pinned   = 0;                     // module self-pin latch

    thread_local bool t_threadHooked = false;         // CALLWNDPROC[RET] installed on this thread?
    thread_local bool t_theming      = false;         // re-entrancy guard (theming sends messages)

    // ---- TEMP DIAGNOSTIC: top-level theming trace -> <target dir>\umbra-inject.log ----
    HANDLE           g_dbg     = INVALID_HANDLE_VALUE;
    CRITICAL_SECTION g_dbgCs{};
    INIT_ONCE        g_dbgOnce = INIT_ONCE_STATIC_INIT;

    BOOL CALLBACK DbgInitOnce(PINIT_ONCE, PVOID, PVOID*) noexcept
    {
        wchar_t path[MAX_PATH]{};
        const DWORD len = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        for (DWORD i = len; i > 0; --i)
            if (path[i - 1] == L'\\') { path[i] = L'\0'; break; }
        ::StringCchCatW(path, ARRAYSIZE(path), L"umbra-inject.log");
        g_dbg = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        ::InitializeCriticalSection(&g_dbgCs);
        return TRUE;
    }

    void Dbg(const wchar_t* line) noexcept
    {
        ::InitOnceExecuteOnce(&g_dbgOnce, DbgInitOnce, nullptr, nullptr);
        if (g_dbg == INVALID_HANDLE_VALUE)
            return;
        char u[512];
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), nullptr, nullptr);
        if (n <= 1)
            return;
        ::EnterCriticalSection(&g_dbgCs);
        DWORD w = 0; ::WriteFile(g_dbg, u, static_cast<DWORD>(n - 1), &w, nullptr); ::FlushFileBuffers(g_dbg);
        ::LeaveCriticalSection(&g_dbgCs);
    }
    // ------------------------------------------------------------------------------------

    const wchar_t* BaseName(const wchar_t* path) noexcept
    {
        const wchar_t* base = path;
        for (const wchar_t* p = path; *p != L'\0'; ++p)
            if (*p == L'\\' || *p == L'/')
                base = p + 1;
        return base;
    }

    bool SameI(const wchar_t* a, const wchar_t* b) noexcept
    {
        return a != nullptr && b != nullptr &&
               ::CompareStringOrdinal(a, -1, b, -1, TRUE) == CSTR_EQUAL;
    }

    void InitProcessFlags() noexcept
    {
        wchar_t path[MAX_PATH]{};
        const DWORD n = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        const wchar_t* name = (n != 0 && n < ARRAYSIZE(path)) ? BaseName(path) : L"";
        g_isTarget = SameI(name, L"regedit.exe") || SameI(name, L"regedt32.exe");
    }

    // Pin so we survive the host's UnhookWindowsHookEx: regedit keeps its theming (and our
    // per-thread CALLWNDPROC hooks stay valid) until it closes, never a dangling pointer.
    void PinModule() noexcept
    {
        if (::InterlockedCompareExchange(&g_pinned, 1, 0) != 0)
            return;
        HMODULE mod = nullptr;
        if (::GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<LPCWSTR>(&PinModule), &mod) == FALSE)
            ::InterlockedExchange(&g_pinned, 0);
    }

    BOOL CALLBACK InitProcessOnce(PINIT_ONCE, PVOID, PVOID*) noexcept
    {
        umbra::initDarkMode();
        umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::dark));
        setProcessWideColorHook();        // GetSysColor / GetSysColorBrush inline (Detours)
        setProcessWideThemeColorHook();   // uxtheme Open/GetThemeColor/DrawThemeBackground (Detours)
        return TRUE;
    }

    // TEMP DIAGNOSTIC: for a top-level window, log its class + whether the DWM dark
    // title-bar attribute is actually set after we themed it. Splits "frame never themed"
    // (no line / hr fail) from "themed but overridden" (dark=1 yet bar still light).
    void TraceTopLevel(HWND hwnd) noexcept
    {
        if ((::GetWindowLongPtrW(hwnd, GWL_STYLE) & WS_CHILD) != 0)
            return;
        wchar_t cls[80] = L"?"; ::GetClassNameW(hwnd, cls, ARRAYSIZE(cls));
        BOOL dark = FALSE;
        const HRESULT hr = ::DwmGetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,
                                                   &dark, sizeof(dark));
        wchar_t line[256];
        ::StringCchPrintfW(line, ARRAYSIZE(line), L"WM_CREATE [TOP] %-22s darkAttr=%d hr=0x%08lX\r\n",
                           cls, static_cast<int>(dark), static_cast<unsigned long>(hr));
        Dbg(line);
    }

    // Per-window theming — identical to sample-hook's AutoDarkMode, minus the EXE-only
    // self-hook/CreateThread machinery (the CBT bootstrap below replaces it).
    LRESULT CALLBACK CallWndProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && !t_theming)
        {
            const auto* info = reinterpret_cast<const CWPSTRUCT*>(lParam);
            if (info->message == WM_NCCREATE && info->hwnd != nullptr)
            {
                t_theming = true;
                umbra::prepDarkModeForNewWindow(info->hwnd);   // early AllowDarkModeForWindow
                t_theming = false;
            }
        }
        return ::CallNextHookEx(nullptr, code, wParam, lParam);
    }

    LRESULT CALLBACK CallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && !t_theming)
        {
            const auto* info = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
            if (info->message == WM_CREATE && info->hwnd != nullptr)
            {
                t_theming = true;
                umbra::applyDarkToNewWindow(info->hwnd);        // full per-window theming
                t_theming = false;
                TraceTopLevel(info->hwnd);                       // TEMP DIAGNOSTIC
            }
            else if (info->message == WM_ACTIVATE && info->hwnd != nullptr
                     && (::GetWindowLongPtrW(info->hwnd, GWL_STYLE) & WS_CHILD) == 0)
            {
                // Some dialogs (comdlg32's Save/Export) clear the dark title-bar attribute
                // during their own init — after our WM_CREATE pass — and repaint the caption
                // light. Re-assert once, when we find it cleared, on activation (after init).
                BOOL dark = FALSE;
                ::DwmGetWindowAttribute(info->hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/,
                                        &dark, sizeof(dark));
                if (!dark && ::GetPropW(info->hwnd, L"umbra.capFix") == nullptr)
                {
                    ::SetPropW(info->hwnd, L"umbra.capFix", reinterpret_cast<HANDLE>(1));
                    t_theming = true;
                    umbra::setDarkTitleBarEx(info->hwnd, true);
                    umbra::redrawWindowFrame(info->hwnd);
                    t_theming = false;

                    wchar_t cls[80] = L"?"; ::GetClassNameW(info->hwnd, cls, ARRAYSIZE(cls));
                    wchar_t line[256];
                    ::StringCchPrintfW(line, ARRAYSIZE(line),
                                       L"WM_ACTIVATE re-assert dark caption (was cleared): %s\r\n", cls);
                    Dbg(line);
                }
            }
        }
        return ::CallNextHookEx(nullptr, code, wParam, lParam);
    }

    // Install the per-window theming hooks on the CURRENT thread, once. Driven by the CBT
    // hook's per-thread firing (every window-creating thread fires CBT) — this is what
    // retires the CreateThread detour + the EXE self-hook trick.
    void EnsureThreadHooks() noexcept
    {
        if (t_threadHooked)
            return;
        t_threadHooked = true;
        const DWORD tid = ::GetCurrentThreadId();
        ::SetWindowsHookExW(WH_CALLWNDPROC,    CallWndProc,    g_self, tid);
        ::SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProc, g_self, tid);
    }
}

extern "C" __declspec(dllexport)
LRESULT CALLBACK UmbraCbtHook(int code, WPARAM wParam, LPARAM lParam)
{
    // First CBT fire on a thread is its first window's HCBT_CREATEWND (before WM_NCCREATE);
    // bootstrap process + thread there so that very window is caught by CallWndProc.
    if (g_isTarget && code >= 0 && !t_theming)
    {
        ::InitOnceExecuteOnce(&g_initOnce, InitProcessOnce, nullptr, nullptr);
        PinModule();
        EnsureThreadHooks();
    }
    return ::CallNextHookEx(nullptr, code, wParam, lParam);
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = module;
        ::DisableThreadLibraryCalls(module);
        InitProcessFlags();
    }
    return TRUE;
}
