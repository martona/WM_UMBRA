// umbra-hook — a color-path probe for the Detours hooking approach.
// -------------------------------------------------------------------------
// A deliberately NEUTRAL harness: plain Win32 / common controls, with NO
// UMBRA theming applied. It renders in the default (light) system theme so
// that, once a process-wide Detours hook on GetSysColor/GetSysColorBrush is
// installed, whatever changes is attributable to the hook alone — nothing else.
//
// Specimens are segregated by color-determination path, one per tab:
//   A  raw user32 / GDI controls   (GetSysColor + WM_CTLCOLOR* — hookable)
//   B  export vs internal          (does the call reach the export at all?)
//   C  comctl32 over uxtheme        (theme-part drawn)
//   E  DirectUI (DUI70)             (Explorer view + modern file dialog)
//
// The Mode menu is a placeholder for now; it tracks the desired light/dark
// state and forces a repaint, which is where the future hook toggle wires in.

// Enable Common-Controls v6 (themed controls) via an embedded manifest dep.
#pragma comment(linker,                                 \
    "\"/manifestdependency:type='win32' "               \
    "name='Microsoft.Windows.Common-Controls' "         \
    "version='6.0.0.0' processorArchitecture='*' "      \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>          // CoInitializeEx / CoUninitialize
#include "hook.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")

HINSTANCE g_hInst = nullptr;

namespace
{
    HWND g_hTab     = nullptr;
    HWND g_hStatus  = nullptr;
    HWND g_hToolbar = nullptr;
    HWND g_hPages[4]{};
    int  g_modeId   = IDM_MODE_SYSTEM;

    struct PageDef { int templ; DLGPROC proc; const wchar_t* label; };
    const PageDef kPages[4] = {
        { IDD_PAGE_A, PageAProc, L"A \x2014 user32/GDI"      },
        { IDD_PAGE_B, PageBProc, L"B \x2014 export/internal" },
        { IDD_PAGE_C, PageCProc, L"C \x2014 uxtheme/comctl"  },
        { IDD_PAGE_E, PageEProc, L"E \x2014 DirectUI"        },
    };

    const wchar_t* ModeLabel()
    {
        switch (g_modeId)
        {
        case IDM_MODE_DARK:  return L"Mode: Dark  (hook not installed yet)";
        case IDM_MODE_LIGHT: return L"Mode: Light (hook not installed yet)";
        default:             return L"Mode: System (hook not installed yet)";
        }
    }

    void UpdateStatus()
    {
        if (g_hStatus != nullptr)
            ::SendMessageW(g_hStatus, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(ModeLabel()));
    }

    void UpdateMenuChecks(HWND hDlg)
    {
        HMENU menu = ::GetMenu(hDlg);
        if (menu != nullptr)
            ::CheckMenuRadioItem(menu, IDM_MODE_SYSTEM, IDM_MODE_LIGHT, g_modeId, MF_BYCOMMAND);
    }

    void CreateBars(HWND hDlg)
    {
        g_hToolbar = ::CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_TOOLBAR), g_hInst, nullptr);

        ::SendMessageW(g_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);
        TBADDBITMAP tbab{ HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
        ::SendMessageW(g_hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&tbab));

        TBBUTTON buttons[] = {
            { STD_PROPERTIES, IDM_HELP_ABOUT, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { STD_FILENEW,    IDM_FILE_EXIT,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        };
        ::SendMessageW(g_hToolbar, TB_ADDBUTTONSW,
            static_cast<WPARAM>(ARRAYSIZE(buttons)), reinterpret_cast<LPARAM>(buttons));
        ::SendMessageW(g_hToolbar, TB_AUTOSIZE, 0, 0);

        g_hStatus = ::CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_STATUSBAR), g_hInst, nullptr);

        int parts[] = { 340, -1 };
        ::SendMessageW(g_hStatus, SB_SETPARTS, ARRAYSIZE(parts), reinterpret_cast<LPARAM>(parts));
        ::SendMessageW(g_hStatus, SB_SETTEXTW, 0,
            reinterpret_cast<LPARAM>(L"umbra-hook \x2014 neutral baseline (no theming)"));
    }

    void CreateTabAndPages(HWND hDlg)
    {
        g_hTab = ::CreateWindowExW(0, WC_TABCONTROLW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_TAB), g_hInst, nullptr);

        // Match the dialog font so the tabs don't render in the bitmap font.
        if (HFONT f = reinterpret_cast<HFONT>(::SendMessageW(hDlg, WM_GETFONT, 0, 0)))
            ::SendMessageW(g_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);

        for (int i = 0; i < 4; ++i)
        {
            TCITEMW it{};
            it.mask = TCIF_TEXT;
            it.pszText = const_cast<LPWSTR>(kPages[i].label);
            ::SendMessageW(g_hTab, TCM_INSERTITEMW, i, reinterpret_cast<LPARAM>(&it));

            // Pages are child dialogs parented to the frame, positioned over the
            // tab's display area. Created hidden (no WS_VISIBLE in the template).
            g_hPages[i] = ::CreateDialogParamW(g_hInst, MAKEINTRESOURCEW(kPages[i].templ),
                hDlg, kPages[i].proc, 0);
        }

        // The tab control was created before the pages, so it sits ABOVE them in
        // the sibling z-order and would paint over the body. Push it to the back;
        // ShowPage() then brings the active page to the front.
        ::SetWindowPos(g_hTab, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void LayoutFrame(HWND hDlg)
    {
        if (g_hTab == nullptr)
            return;

        RECT rc;
        ::GetClientRect(hDlg, &rc);

        ::SendMessageW(g_hToolbar, TB_AUTOSIZE, 0, 0);
        RECT tb{};
        ::GetWindowRect(g_hToolbar, &tb);
        const int tbH = tb.bottom - tb.top;

        ::SendMessageW(g_hStatus, WM_SIZE, 0, 0);
        RECT sb{};
        ::GetWindowRect(g_hStatus, &sb);
        const int sbH = sb.bottom - sb.top;

        const int tabY = tbH;
        int tabH = rc.bottom - tbH - sbH;
        if (tabH < 0) tabH = 0;
        ::MoveWindow(g_hTab, 0, tabY, rc.right, tabH, TRUE);

        RECT disp{ 0, tabY, rc.right, tabY + tabH };
        ::SendMessageW(g_hTab, TCM_ADJUSTRECT, FALSE, reinterpret_cast<LPARAM>(&disp));
        for (int i = 0; i < 4; ++i)
        {
            if (g_hPages[i] != nullptr)
                ::MoveWindow(g_hPages[i], disp.left, disp.top,
                    disp.right - disp.left, disp.bottom - disp.top, TRUE);
        }
    }

    void ShowPage(int sel)
    {
        for (int i = 0; i < 4; ++i)
            if (g_hPages[i] != nullptr)
                ::ShowWindow(g_hPages[i], i == sel ? SW_SHOW : SW_HIDE);

        // Bring the active page in front of the tab control (see CreateTabAndPages).
        if (sel >= 0 && sel < 4 && g_hPages[sel] != nullptr)
            ::SetWindowPos(g_hPages[sel], HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

INT_PTR CALLBACK FrameProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        CreateBars(hDlg);
        CreateTabAndPages(hDlg);
        UpdateStatus();
        UpdateMenuChecks(hDlg);
        LayoutFrame(hDlg);
        TabCtrl_SetCurSel(g_hTab, 0);
        ShowPage(0);
        return TRUE;

    case WM_SIZE:
        LayoutFrame(hDlg);
        return FALSE;

    case WM_GETMINMAXINFO:
    {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 540;
        mmi->ptMinTrackSize.y = 440;
        return TRUE;
    }

    case WM_NOTIFY:
    {
        auto* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr->hwndFrom == g_hTab && nmhdr->code == TCN_SELCHANGE)
        {
            ShowPage(TabCtrl_GetCurSel(g_hTab));
            return TRUE;
        }
        return FALSE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDM_HELP_ABOUT:
            ::MessageBoxW(hDlg,
                L"umbra-hook\n"
                L"Neutral color-path probe for the Detours hook experiment.\n\n"
                L"No theming is applied: the harness renders in the default theme\n"
                L"so that anything the hook changes is attributable to the hook.\n\n"
                L"Tabs: A user32 \x00B7 B export/internal \x00B7 C uxtheme \x00B7 E DirectUI.",
                L"About umbra-hook", MB_OK | MB_ICONINFORMATION);
            return TRUE;

        // The Mode items currently only record intent + repaint. The Detours
        // hook (next phase) will read this state to decide what color to return.
        case IDM_MODE_SYSTEM:
        case IDM_MODE_DARK:
        case IDM_MODE_LIGHT:
            g_modeId = static_cast<int>(LOWORD(wParam));
            UpdateStatus();
            UpdateMenuChecks(hDlg);
            ::RedrawWindow(hDlg, nullptr, nullptr,
                RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
            return TRUE;

        case IDM_FILE_EXIT:
            ::DestroyWindow(hDlg);
            return TRUE;
        }
        return FALSE;

    case WM_CLOSE:
        ::DestroyWindow(hDlg);
        return TRUE;

    case WM_DESTROY:
        ::PostQuitMessage(0);
        return TRUE;
    }
    return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow)
{
    g_hInst = hInstance;

    const HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    INITCOMMONCONTROLSEX icc{ sizeof(icc),
        ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES |
        ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_UPDOWN_CLASS |
        ICC_DATE_CLASSES | ICC_HOTKEY_CLASS | ICC_INTERNET_CLASSES | ICC_LINK_CLASS |
        ICC_USEREX_CLASSES | ICC_COOL_CLASSES };
    ::InitCommonControlsEx(&icc);

    // NOTE: no umbra::initDarkMode() — this harness is intentionally untouched by
    // UMBRA. The Detours hook installer will go here in the next phase.

    HWND hDlg = ::CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_FRAME),
        nullptr, FrameProc, 0);
    if (hDlg == nullptr)
    {
        if (SUCCEEDED(hrCo)) ::CoUninitialize();
        return 1;
    }

    ::ShowWindow(hDlg, nCmdShow);

    MSG msg{};
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0)
    {
        if (::IsDialogMessageW(hDlg, &msg) == 0)
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    if (SUCCEEDED(hrCo)) ::CoUninitialize();
    return static_cast<int>(msg.wParam);
}
