// SPDX-License-Identifier: MIT
//
// AutoDarkMode.cpp — automatic, process-wide per-window dark theming.
//   (umbra-hook harness — the application / interception half.)
//   (Carrying TEMP DIAGNOSTIC logging to autodark.log — kept for now.)
//
// The reach mechanism that used to live in the umbra library. The theming
// DECISION for each freshly-created window is the library's
// (umbra::applyDarkToNewWindow); this file owns only how that decision reaches
// every window on every thread, so the Detours/hook machinery stays out of umbra.
//
// Two thread-local hooks per thread: WH_CALLWNDPROC (fires BEFORE the window proc —
// used on WM_NCCREATE for an early AllowDarkModeForWindow so DUI/uxtheme picks the dark
// theme variant) and WH_CALLWNDPROCRET (fires AFTER — used on WM_CREATE for the full
// per-window theming). A hook proc that lives in the EXE (not a DLL) may only be
// installed on the CALLING thread — installing it on another thread fails with
// ERROR_INVALID_PARAMETER. So each thread hooks ITSELF: the
// CreateThread detour wraps the start routine, and the wrapper (which runs on
// the new thread) installs the hook on the current thread before calling the
// real routine. setAutoDarkMode is called before any thread is spawned, so the
// main-thread install + the detour together cover every thread, no DLL.

#include <windows.h>
#include <detours/detours.h>
#include <strsafe.h>
#include <new>
#include "umbra.h"
#include "hook.h"

#pragma comment(lib, "detours.lib")

namespace
{
    HHOOK g_hook         = nullptr;   // WH_CALLWNDPROCRET — full theming, after WM_CREATE
    HHOOK g_hookProc     = nullptr;   // WH_CALLWNDPROC — early AllowDarkModeForWindow, on WM_NCCREATE
    bool  g_threadDetour = false;
    thread_local bool g_theming = false;

    // ---- TEMP DIAGNOSTIC -------------------------------------------------
    HANDLE           g_dbg = INVALID_HANDLE_VALUE;
    CRITICAL_SECTION g_dbgCs;
    bool             g_dbgReady = false;

    void DbgInit()
    {
        wchar_t path[MAX_PATH];
        umbraLogPath(L"autodark.log", path, ARRAYSIZE(path));
        g_dbg = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        ::InitializeCriticalSection(&g_dbgCs);
        g_dbgReady = true;
    }
    void Dbg(const wchar_t* line)
    {
        if (!g_dbgReady || g_dbg == INVALID_HANDLE_VALUE) return;
        char u[512];
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, line, -1, u, sizeof(u), nullptr, nullptr);
        if (n <= 1) return;
        ::EnterCriticalSection(&g_dbgCs);
        DWORD w = 0; ::WriteFile(g_dbg, u, static_cast<DWORD>(n - 1), &w, nullptr); ::FlushFileBuffers(g_dbg);
        ::LeaveCriticalSection(&g_dbgCs);
    }
    // ----------------------------------------------------------------------

    // WH_CALLWNDPROC fires BEFORE the window proc handles the message — so on WM_NCCREATE
    // (the window's first message) we allow dark mode before the window opens its theme,
    // which the after-the-fact WH_CALLWNDPROCRET pass may be too late for on DUI views.
    LRESULT CALLBACK CallWndProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && !g_theming)
        {
            const auto* info = reinterpret_cast<const CWPSTRUCT*>(lParam);
            if (info->message == WM_NCCREATE && info->hwnd != nullptr)
            {
                wchar_t cls[80] = L"?"; ::GetClassNameW(info->hwnd, cls, ARRAYSIZE(cls));
                wchar_t line[256];
                ::StringCchPrintfW(line, ARRAYSIZE(line), L"CALLWNDPROC tid=%lu  WM_NCCREATE %s\r\n",
                    ::GetCurrentThreadId(), cls);
                Dbg(line);

                g_theming = true;
                umbra::prepDarkModeForNewWindow(info->hwnd);
                g_theming = false;
            }
        }
        return ::CallNextHookEx(nullptr, code, wParam, lParam);
    }

    LRESULT CALLBACK CallWndRetProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HC_ACTION && !g_theming)
        {
            const auto* info = reinterpret_cast<const CWPRETSTRUCT*>(lParam);
            if (info->message == WM_CREATE && info->hwnd != nullptr)
            {
                wchar_t cls[80] = L"?"; ::GetClassNameW(info->hwnd, cls, ARRAYSIZE(cls));
                wchar_t line[256];
                ::StringCchPrintfW(line, ARRAYSIZE(line), L"CALLWNDRET tid=%lu  WM_CREATE %s\r\n",
                    ::GetCurrentThreadId(), cls);
                Dbg(line);

                // The library owns the per-window theming decision; we only drive it.
                g_theming = true;
                umbra::applyDarkToNewWindow(info->hwnd);
                g_theming = false;
            }
        }
        return ::CallNextHookEx(nullptr, code, wParam, lParam);
    }

    // Install the hook on the CURRENT thread (the only install a non-DLL proc is
    // allowed to do). Per-thread hooks self-remove when the thread exits.
    void InstallSelfHook()
    {
        const DWORD tid    = ::GetCurrentThreadId();
        const HHOOK hkProc = ::SetWindowsHookExW(WH_CALLWNDPROC,    CallWndProc,    nullptr, tid);
        const HHOOK hkRet  = ::SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProc, nullptr, tid);
        wchar_t line[200];
        ::StringCchPrintfW(line, ARRAYSIZE(line), L"SELFHOOK tid=%lu  proc=%p ret=%p  err=%lu\r\n",
            tid, reinterpret_cast<void*>(hkProc), reinterpret_cast<void*>(hkRet),
            (hkProc != nullptr && hkRet != nullptr) ? 0UL : ::GetLastError());
        Dbg(line);
    }

    // Each thread runs this first, hooks itself, then runs the real start routine.
    struct ThreadCtx { LPTHREAD_START_ROUTINE start; LPVOID param; };

    DWORD WINAPI ThreadProxy(LPVOID p)
    {
        ThreadCtx ctx = *static_cast<ThreadCtx*>(p);
        delete static_cast<ThreadCtx*>(p);
        InstallSelfHook();
        return ctx.start(ctx.param);
    }

    using fnCreateThread = HANDLE (WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T,
        LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    fnCreateThread g_realCreateThread = ::CreateThread;

    HANDLE WINAPI MyCreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack,
        LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD pTid)
    {
        auto* ctx = new (std::nothrow) ThreadCtx{ start, param };
        if (ctx == nullptr)
            return g_realCreateThread(sa, stack, start, param, flags, pTid);   // unwrapped fallback

        HANDLE h = g_realCreateThread(sa, stack, ThreadProxy, ctx, flags, pTid);
        if (h == nullptr)
            delete ctx;
        return h;
    }
}

bool setAutoDarkMode() noexcept
{
    if (g_hook != nullptr)
        return true;

    DbgInit();

    g_hookProc = ::SetWindowsHookExW(WH_CALLWNDPROC, CallWndProc,
        nullptr, ::GetCurrentThreadId());
    g_hook = ::SetWindowsHookExW(WH_CALLWNDPROCRET, CallWndRetProc,
        nullptr, ::GetCurrentThreadId());

    if (::DetourTransactionBegin() == NO_ERROR)
    {
        ::DetourUpdateThread(::GetCurrentThread());
        ::DetourAttach(reinterpret_cast<PVOID*>(&g_realCreateThread),
                       reinterpret_cast<PVOID>(MyCreateThread));
        g_threadDetour = (::DetourTransactionCommit() == NO_ERROR);
    }

    wchar_t line[160];
    ::StringCchPrintfW(line, ARRAYSIZE(line),
        L"SETAUTODARKMODE maintid=%lu hook=%p threadDetour=%d\r\n",
        ::GetCurrentThreadId(), reinterpret_cast<void*>(g_hook), g_threadDetour ? 1 : 0);
    Dbg(line);

    return g_hook != nullptr;
}

void unsetAutoDarkMode() noexcept
{
    if (g_threadDetour)
    {
        if (::DetourTransactionBegin() == NO_ERROR)
        {
            ::DetourUpdateThread(::GetCurrentThread());
            ::DetourDetach(reinterpret_cast<PVOID*>(&g_realCreateThread),
                           reinterpret_cast<PVOID>(MyCreateThread));
            ::DetourTransactionCommit();
        }
        g_threadDetour = false;
    }

    if (g_hookProc != nullptr)
    {
        ::UnhookWindowsHookEx(g_hookProc);
        g_hookProc = nullptr;
    }

    if (g_hook != nullptr)
    {
        ::UnhookWindowsHookEx(g_hook);
        g_hook = nullptr;
    }

    if (g_dbgReady)
    {
        if (g_dbg != INVALID_HANDLE_VALUE) { ::CloseHandle(g_dbg); g_dbg = INVALID_HANDLE_VALUE; }
        ::DeleteCriticalSection(&g_dbgCs);
        g_dbgReady = false;
    }
}
