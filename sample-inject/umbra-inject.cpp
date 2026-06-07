// SPDX-License-Identifier: MIT
//
// umbra-inject.exe — host / controller for the umbra dark-theming payload.
//
// Installs ONE global WH_CBT hook whose proc lives in umbra-payload.dll, then stays
// alive (pumping messages) so the hook stays live. WH_CBT maps the payload into each
// target process as early as its first window; the payload whitelists regedit.exe and
// themes from there. uiAccess (asInvoker + signed, see umbra-inject.vcxproj) lets this
// medium-integrity host reach an elevated regedit without elevating itself — granted
// only to a signed binary in a trusted path (we report whether we actually got it).
//
// Temporary in-tree sample; removed before this branch merges to umbra's main.

#include <cstdio>
#include <string>
#include <thread>

#include <windows.h>

namespace
{
    HANDLE g_stop = nullptr;

    std::wstring ModuleDir()
    {
        wchar_t path[MAX_PATH]{};
        const DWORD n = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
        std::wstring p(path, (n != 0 && n < ARRAYSIZE(path)) ? n : 0);
        const size_t slash = p.find_last_of(L"\\/");
        return slash == std::wstring::npos ? L"." : p.substr(0, slash);
    }

    // Did we actually receive uiAccess? (Only granted to a signed binary in a trusted
    // path; otherwise the manifest request is silently dropped and we run medium.)
    bool HasUiAccess()
    {
        HANDLE token = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
            return false;
        DWORD uiAccess = 0;
        DWORD returned = 0;
        const bool ok =
            ::GetTokenInformation(token, TokenUIAccess, &uiAccess, sizeof(uiAccess), &returned) != FALSE;
        ::CloseHandle(token);
        return ok && uiAccess != 0;
    }

    BOOL WINAPI ConsoleHandler(DWORD)
    {
        if (g_stop != nullptr)
            ::SetEvent(g_stop);
        return TRUE;
    }

    void PumpUntilStop()
    {
        MSG msg{};
        for (;;)
        {
            const DWORD wait = ::MsgWaitForMultipleObjects(1, &g_stop, FALSE, INFINITE, QS_ALLINPUT);
            if (wait == WAIT_OBJECT_0)
                return;
            if (wait != WAIT_OBJECT_0 + 1)
                return;
            while (::PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&msg);
                ::DispatchMessageW(&msg);
            }
        }
    }
}

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    std::printf("umbra-inject - dark-theming host (stub: regedit title bar)\n");
    std::printf("uiAccess: %s\n\n",
                HasUiAccess() ? "granted" : "NO (run signed from a trusted path, or elevate)");

    const std::wstring dllPath = ModuleDir() + L"\\umbra-payload.dll";
    HMODULE hookModule = ::LoadLibraryW(dllPath.c_str());
    if (hookModule == nullptr)
    {
        std::fwprintf(stderr, L"LoadLibrary(%s) failed: %lu\n", dllPath.c_str(), ::GetLastError());
        return 1;
    }

    const auto cbtProc = reinterpret_cast<HOOKPROC>(::GetProcAddress(hookModule, "UmbraCbtHook"));
    if (cbtProc == nullptr)
    {
        std::fwprintf(stderr, L"GetProcAddress(UmbraCbtHook) failed: %lu\n", ::GetLastError());
        ::FreeLibrary(hookModule);
        return 1;
    }

    g_stop = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_stop == nullptr)
    {
        ::FreeLibrary(hookModule);
        return 1;
    }
    ::SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    const HHOOK cbtHook = ::SetWindowsHookExW(WH_CBT, cbtProc, hookModule, 0);
    if (cbtHook == nullptr)
    {
        std::fwprintf(stderr, L"SetWindowsHookEx(WH_CBT) failed: %lu\n", ::GetLastError());
        ::SetConsoleCtrlHandler(ConsoleHandler, FALSE);
        ::CloseHandle(g_stop);
        ::FreeLibrary(hookModule);
        return 1;
    }

    std::printf("WH_CBT installed. Open regedit; its title bar should go dark.\n");
    std::printf("Press Enter to unhook and exit.\n");

    std::thread inputThread([]()
    {
        (void)std::getchar();
        if (g_stop != nullptr)
            ::SetEvent(g_stop);
    });

    PumpUntilStop();

    if (inputThread.joinable())
        inputThread.detach();

    ::UnhookWindowsHookEx(cbtHook);
    ::SetConsoleCtrlHandler(ConsoleHandler, FALSE);
    ::CloseHandle(g_stop);
    g_stop = nullptr;

    // Passive cleanup: the payload stays pinned in any process it themed (its subclasses
    // self-remove on WM_NCDESTROY). We just release our own reference.
    ::FreeLibrary(hookModule);
    std::printf("Unhooked. Already-themed windows keep their theme until they close.\n");
    return 0;
}
