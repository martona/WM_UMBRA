// umbra-hook — Tab B: the export-vs-internal exhibit.
//
// Two child windows, identical in appearance today, that fill their background
// by two different routes. The point is realized once a GetSysColor/Brush hook
// exists: it should darken ONLY the panel that calls the exported API.
//
//   EXPORT panel   — WM_ERASEBKGND calls GetSysColorBrush(COLOR_WINDOW).
//                    A hook on the user32 export reaches this fill.
//   INTERNAL panel — its WNDCLASS.hbrBackground is (COLOR_WINDOW+1); user32
//                    resolves that class brush *internally* when DefWindowProc
//                    erases, which a hook on the export does not see.
//
// If the internal panel ALSO darkens once hooked, that's an empirical finding
// about where user32's internal class-brush resolution routes on this build.
//
// Both panels draw their caption with GetSysColor(COLOR_WINDOWTEXT) — itself an
// export call — so only the *background* mechanism differs between them.
//
// These use private window classes, so UMBRA's per-window theming walk (which
// targets known control classes) leaves them alone; they show raw behavior.

#include <windows.h>
#include "hook.h"
#include "resource.h"

namespace
{
    const wchar_t* kExportCls   = L"UmbraHookExportPanel";
    const wchar_t* kInternalCls = L"UmbraHookInternalPanel";

    void PaintCaption(HWND hWnd, const wchar_t* text)
    {
        PAINTSTRUCT ps;
        HDC hdc = ::BeginPaint(hWnd, &ps);
        RECT rc;
        ::GetClientRect(hWnd, &rc);
        rc.left += 8; rc.top += 8; rc.right -= 8;
        ::SetBkMode(hdc, TRANSPARENT);
        ::SetTextColor(hdc, ::GetSysColor(COLOR_WINDOWTEXT));   // export call
        ::DrawTextW(hdc, text, -1, &rc, DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
        ::EndPaint(hWnd, &ps);
    }

    LRESULT CALLBACK ExportProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
        case WM_ERASEBKGND:
        {
            RECT rc;
            ::GetClientRect(hWnd, &rc);
            ::FillRect(reinterpret_cast<HDC>(wParam), &rc, ::GetSysColorBrush(COLOR_WINDOW));
            return 1;   // handled via the exported GetSysColorBrush
        }
        case WM_PAINT:
            PaintCaption(hWnd,
                L"EXPORT path\r\n\r\nGetSysColorBrush(COLOR_WINDOW)\r\n"
                L"\x2192 a hook on the export reaches this fill.");
            return 0;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    LRESULT CALLBACK InternalProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        // WM_ERASEBKGND deliberately falls through to DefWindowProc, which erases
        // using the class background brush (COLOR_WINDOW+1) resolved internally.
        if (msg == WM_PAINT)
        {
            PaintCaption(hWnd,
                L"INTERNAL path\r\n\r\nclass brush (COLOR_WINDOW+1)\r\n"
                L"\x2192 user32 erases this internally.");
            return 0;
        }
        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    void EnsureClasses()
    {
        static bool registered = false;
        if (registered)
            return;

        WNDCLASSW wc{};
        wc.lpfnWndProc   = ExportProc;
        wc.hInstance     = g_hInst;
        wc.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = nullptr;            // we fill it ourselves, via the export
        wc.lpszClassName = kExportCls;
        ::RegisterClassW(&wc);

        WNDCLASSW wci{};
        wci.lpfnWndProc   = InternalProc;
        wci.hInstance     = g_hInst;
        wci.hCursor       = ::LoadCursorW(nullptr, IDC_ARROW);
        wci.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);  // internal resolution
        wci.lpszClassName = kInternalCls;
        ::RegisterClassW(&wci);

        registered = true;
    }
}

void CreateBPanels(HWND page)
{
    EnsureClasses();

    ::CreateWindowExW(WS_EX_CLIENTEDGE, kExportCls, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10, page, reinterpret_cast<HMENU>(IDC_B_PANEL_EXPORT), g_hInst, nullptr);
    ::CreateWindowExW(WS_EX_CLIENTEDGE, kInternalCls, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 10, 10, page, reinterpret_cast<HMENU>(IDC_B_PANEL_INTERNAL), g_hInst, nullptr);

    LayoutBPanels(page);
}

void LayoutBPanels(HWND page)
{
    HWND pe = ::GetDlgItem(page, IDC_B_PANEL_EXPORT);
    HWND pi = ::GetDlgItem(page, IDC_B_PANEL_INTERNAL);
    if (pe == nullptr || pi == nullptr)
        return;

    RECT rc;
    ::GetClientRect(page, &rc);

    // Sit the panels below the two descriptive labels.
    int top = 70;
    if (HWND lbl = ::GetDlgItem(page, IDC_B_LBL_EXPORT))
    {
        RECT lr;
        ::GetWindowRect(lbl, &lr);
        ::MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&lr), 2);
        top = lr.bottom + 6;
    }

    const int margin = 8;
    const int gap    = 10;
    int w = (rc.right - margin * 2 - gap) / 2;
    int h = rc.bottom - top - margin;
    if (w < 10) w = 10;
    if (h < 10) h = 10;

    ::MoveWindow(pe, margin, top, w, h, TRUE);
    ::MoveWindow(pi, margin + w + gap, top, w, h, TRUE);
    ::InvalidateRect(pe, nullptr, TRUE);
    ::InvalidateRect(pi, nullptr, TRUE);
}
