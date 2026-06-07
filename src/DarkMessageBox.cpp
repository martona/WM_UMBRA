/*
* MIT License
*
* Copyright (C) 2026 Marton Anka
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include <windows.h>
#include <cwchar>
#include <cstdlib>    // _countof — windows.h is trimmed here by VC_EXTRALEAN
#include <commctrl.h>
#include "umbra.h"

#pragma comment(lib, "comctl32.lib")

namespace
{
    thread_local HHOOK g_msgBoxHook = nullptr;
    thread_local bool  g_msgBoxDarkened = false;

    bool IsDialogWindow(HWND hwnd)
    {
        wchar_t cls[32]{};
        ::GetClassNameW(hwnd, cls, _countof(cls));
        return std::wcscmp(cls, L"#32770") == 0;
    }

    bool AppWantsDarkMessageBox()
    {
        return umbra::isEnabled();
    }

    void DarkenMessageBoxWindow(HWND hwnd)
    {
        if (!hwnd || g_msgBoxDarkened || !AppWantsDarkMessageBox())
            return;

        g_msgBoxDarkened = true;

        umbra::setWindowEraseBgSubclass(hwnd);
        umbra::setDarkWndNotifySafe(hwnd, true);

        // Disable any themed dialog texture nonsense.
        umbra::enableThemeDialogTexture(hwnd, false);

        // Must be installed after the umbra subclasses (outermost): this lets the
        // backfill's WM_PAINT run DefSubclassProc() first, then fill the leftover band.
        umbra::setWindowBackfillSubclass(hwnd);

        ::RedrawWindow(
            hwnd,
            nullptr,
            nullptr,
            RDW_INVALIDATE |
            RDW_ERASE |
            RDW_ALLCHILDREN |
            RDW_FRAME |
            RDW_UPDATENOW);
    }

    LRESULT CALLBACK MessageBoxCbtProc(int code, WPARAM wParam, LPARAM lParam)
    {
        if (code == HCBT_ACTIVATE)
        {
            HWND hwnd = reinterpret_cast<HWND>(wParam);

            if (IsDialogWindow(hwnd))
                DarkenMessageBoxWindow(hwnd);
        }

        return ::CallNextHookEx(g_msgBoxHook, code, wParam, lParam);
    }

    class ScopedMessageBoxHook
    {
    public:
        ScopedMessageBoxHook()
        {
            g_msgBoxDarkened = false;

            // Thread hook only
            g_msgBoxHook = ::SetWindowsHookExW(
                WH_CBT,
                MessageBoxCbtProc,
                nullptr,
                ::GetCurrentThreadId());
        }

        ~ScopedMessageBoxHook()
        {
            if (g_msgBoxHook)
            {
                ::UnhookWindowsHookEx(g_msgBoxHook);
                g_msgBoxHook = nullptr;
            }

            g_msgBoxDarkened = false;
        }

        ScopedMessageBoxHook(const ScopedMessageBoxHook&) = delete;
        ScopedMessageBoxHook& operator=(const ScopedMessageBoxHook&) = delete;
    };
}

namespace umbra
{
    int DarkMessageBox(HWND owner, LPCWSTR text, LPCWSTR caption, UINT type)
    {
        ScopedMessageBoxHook hook;

        return ::MessageBoxW(
            owner,
            text,
            caption,
            type);
    }
}
