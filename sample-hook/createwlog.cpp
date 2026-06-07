// createwlog.cpp — THROWAWAY instrumentation (not part of the library).
//
// Answers: which thread hosts a given dialog (e.g. the shell Permissions /
// Advanced Security sheet), and was that thread one we ever saw created? Logs to
// <exe dir>\createwlog.txt:
//   MAIN tid / pid + every thread that ALREADY exists at startup,
//   every thread CreateThread produces (new tid + creator tid),
//   every window creation, tagged with the creating thread id.

#include <windows.h>
#include <detours/detours.h>
#include <tlhelp32.h>
#include <strsafe.h>
#include "hook.h"

#pragma comment(lib, "detours.lib")

namespace
{
    HANDLE           g_log = INVALID_HANDLE_VALUE;
    CRITICAL_SECTION g_cs;
    bool             g_on = false;

    void WriteLineW(const wchar_t* line)
    {
        if (g_log == INVALID_HANDLE_VALUE)
            return;
        char utf8[1024];
        const int n = ::WideCharToMultiByte(CP_UTF8, 0, line, -1, utf8, sizeof(utf8), nullptr, nullptr);
        if (n <= 1)
            return;
        ::EnterCriticalSection(&g_cs);
        DWORD w = 0;
        ::WriteFile(g_log, utf8, static_cast<DWORD>(n - 1), &w, nullptr);
        ::FlushFileBuffers(g_log);
        ::LeaveCriticalSection(&g_cs);
    }

    // --- CreateWindowEx: log window class + the creating thread id ----------
    using fnCWW = HWND (WINAPI*)(DWORD, LPCWSTR, LPCWSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    using fnCWA = HWND (WINAPI*)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
    fnCWW g_realCWW = ::CreateWindowExW;
    fnCWA g_realCWA = ::CreateWindowExA;

    void LogWin(HWND h, HWND parent, char src)
    {
        wchar_t cls[96] = L"?";
        if (h != nullptr) ::GetClassNameW(h, cls, ARRAYSIZE(cls));
        wchar_t pcls[96] = L"-";
        if (parent != nullptr) ::GetClassNameW(parent, pcls, ARRAYSIZE(pcls));
        wchar_t line[512];
        if (SUCCEEDED(::StringCchPrintfW(line, ARRAYSIZE(line),
                L"[%c] tid=%-6lu  %-22s  parent=(%s)\r\n",
                src, ::GetCurrentThreadId(), cls, pcls)))
            WriteLineW(line);
    }

    HWND WINAPI MyCWW(DWORD e, LPCWSTR c, LPCWSTR n, DWORD s, int x, int y, int w, int h, HWND p, HMENU m, HINSTANCE i, LPVOID l)
    {
        HWND r = g_realCWW(e, c, n, s, x, y, w, h, p, m, i, l);
        LogWin(r, p, 'W');
        return r;
    }
    HWND WINAPI MyCWA(DWORD e, LPCSTR c, LPCSTR n, DWORD s, int x, int y, int w, int h, HWND p, HMENU m, HINSTANCE i, LPVOID l)
    {
        HWND r = g_realCWA(e, c, n, s, x, y, w, h, p, m, i, l);
        LogWin(r, p, 'A');
        return r;
    }

    // --- CreateThread: log every thread the process makes -------------------
    using fnCT = HANDLE (WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD);
    fnCT g_realCT = ::CreateThread;

    HANDLE WINAPI MyCT(LPSECURITY_ATTRIBUTES sa, SIZE_T st, LPTHREAD_START_ROUTINE fn, LPVOID pr, DWORD fl, LPDWORD pt)
    {
        HANDLE h = g_realCT(sa, st, fn, pr, fl, pt);
        if (h != nullptr)
        {
            wchar_t line[160];
            if (SUCCEEDED(::StringCchPrintfW(line, ARRAYSIZE(line),
                    L"CREATETHREAD newtid=%lu  (by tid=%lu)\r\n",
                    ::GetThreadId(h), ::GetCurrentThreadId())))
                WriteLineW(line);
        }
        return h;
    }

    void LogStartup()
    {
        wchar_t line[160];
        if (SUCCEEDED(::StringCchPrintfW(line, ARRAYSIZE(line),
                L"=== createwlog ===  MAIN tid=%lu pid=%lu\r\n",
                ::GetCurrentThreadId(), ::GetCurrentProcessId())))
            WriteLineW(line);

        HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snap != INVALID_HANDLE_VALUE)
        {
            const DWORD pid = ::GetCurrentProcessId();
            THREADENTRY32 te{ sizeof(te) };
            if (::Thread32First(snap, &te))
            {
                do {
                    if (te.th32OwnerProcessID == pid)
                    {
                        if (SUCCEEDED(::StringCchPrintfW(line, ARRAYSIZE(line),
                                L"STARTUP-THREAD tid=%lu\r\n", te.th32ThreadID)))
                            WriteLineW(line);
                    }
                } while (::Thread32Next(snap, &te));
            }
            ::CloseHandle(snap);
        }
    }
}

void StartCreateWindowLog()
{
    if (g_on)
        return;

    wchar_t path[MAX_PATH];
    const DWORD len = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    for (DWORD i = len; i > 0; --i)
        if (path[i - 1] == L'\\') { path[i] = L'\0'; break; }
    ::StringCchCatW(path, ARRAYSIZE(path), L"createwlog.txt");

    g_log = ::CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (g_log == INVALID_HANDLE_VALUE)
        return;

    ::InitializeCriticalSection(&g_cs);
    LogStartup();

    if (::DetourTransactionBegin() != NO_ERROR)
        return;
    ::DetourUpdateThread(::GetCurrentThread());
    ::DetourAttach(reinterpret_cast<PVOID*>(&g_realCWW), reinterpret_cast<PVOID>(MyCWW));
    ::DetourAttach(reinterpret_cast<PVOID*>(&g_realCWA), reinterpret_cast<PVOID>(MyCWA));
    ::DetourAttach(reinterpret_cast<PVOID*>(&g_realCT),  reinterpret_cast<PVOID>(MyCT));
    if (::DetourTransactionCommit() == NO_ERROR)
        g_on = true;
}

void StopCreateWindowLog()
{
    if (!g_on)
        return;
    ::DetourTransactionBegin();
    ::DetourUpdateThread(::GetCurrentThread());
    ::DetourDetach(reinterpret_cast<PVOID*>(&g_realCWW), reinterpret_cast<PVOID>(MyCWW));
    ::DetourDetach(reinterpret_cast<PVOID*>(&g_realCWA), reinterpret_cast<PVOID>(MyCWA));
    ::DetourDetach(reinterpret_cast<PVOID*>(&g_realCT),  reinterpret_cast<PVOID>(MyCT));
    ::DetourTransactionCommit();
    g_on = false;
    ::DeleteCriticalSection(&g_cs);
    if (g_log != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(g_log);
        g_log = INVALID_HANDLE_VALUE;
    }
}
