// umbra-hook — a color-path probe for the Detours hooking approach.
// -------------------------------------------------------------------------
// The harness applies dark mode automatically, with no per-window calls in app
// code: the harness's own setAutoDarkMode() (AutoDarkMode.cpp) installs a
// WH_CALLWNDPROCRET hook that themes each window (via umbra::applyDarkToNewWindow)
// as it is created, and setProcessWideColorHook() (ProcessColorHook.cpp) covers
// the classic GetSysColor / DirectUI residue. The interception machinery (Detours)
// lives here in the app; UMBRA supplies the per-window / per-color theming
// decisions (applyDarkToNewWindow / darkSysColor).
//
// Specimens are segregated by color-determination path, one per tab:
//   A  raw user32 / GDI controls   (GetSysColor + WM_CTLCOLOR* — hookable)
//   B  export vs internal          (does the call reach the export at all?)
//   C  comctl32 over uxtheme        (theme-part drawn)
//   E  DirectUI (DUI70)             (Explorer view + modern file/security dlgs)
//
// We populate UMBRA's palette as plain data (setDarkModeConfig + setDefault
// Colors) so the hook has dark values to serve — but we deliberately do NOT
// call initDarkMode() (which would also install the comctl32 IAT hook and the
// experimental app-mode) nor any setDarkWndNotifySafe theming. The hook is
// installed before the window tree is created (so it is built under the hook),
// and the Mode menu's "color hook" item toggles it at runtime.

// Enable Common-Controls v6 (themed controls) via an embedded manifest dep.
#pragma comment(linker,                                 \
    "\"/manifestdependency:type='win32' "               \
    "name='Microsoft.Windows.Common-Controls' "         \
    "version='6.0.0.0' processorArchitecture='*' "      \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>          // CoInitializeEx / CoUninitialize
#include "umbra.h"
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
    int  g_modeId   = IDM_MODE_DARK;   // which palette the color hook serves
    bool g_hookOn       = false;       // GetSysColor/Brush hook installed (runtime-toggleable)
    bool g_themeColorOn = false;       // uxtheme GetThemeColor probe installed (startup)
    bool g_themeOn      = false;       // auto dark-mode per-window hook installed (startup)

    struct PageDef { int templ; DLGPROC proc; const wchar_t* label; };
    const PageDef kPages[4] = {
        { IDD_PAGE_A, PageAProc, L"A \x2014 user32/GDI"      },
        { IDD_PAGE_B, PageBProc, L"B \x2014 export/internal" },
        { IDD_PAGE_C, PageCProc, L"C \x2014 uxtheme/comctl"  },
        { IDD_PAGE_E, PageEProc, L"E \x2014 DirectUI"        },
    };

    // Populate UMBRA's palette (plain data — no theming, no IAT hook, no
    // experimental app-mode). This is what the process-wide hook serves.
    void ApplyPalette(int modeId)
    {
        g_modeId = modeId;
        switch (modeId)
        {
        case IDM_MODE_DARK:  umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::dark));    break;
        case IDM_MODE_LIGHT: umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::classic)); break;
        default:             umbra::setDarkModeConfig();  break;   // follow the OS setting
        }
        umbra::setDefaultColors(true);
    }

    const wchar_t* PaletteLabel()
    {
        switch (g_modeId)
        {
        case IDM_MODE_DARK:  return L"Dark";
        case IDM_MODE_LIGHT: return L"Light";
        default:             return L"System";
        }
    }

    void UpdateStatus()
    {
        if (g_hStatus != nullptr)
        {
            wchar_t buf[200];
            ::wsprintfW(buf, L"Palette: %s   \x00B7   Color: %s   \x00B7   Theme: %s   \x00B7   Auto: %s",
                PaletteLabel(),
                g_hookOn ? L"ON" : L"OFF",
                g_themeColorOn ? L"ON" : L"OFF",
                g_themeOn ? L"ON" : L"OFF");
            ::SendMessageW(g_hStatus, SB_SETTEXTW, 1, reinterpret_cast<LPARAM>(buf));
        }
    }

    void UpdateMenuChecks(HWND hDlg)
    {
        HMENU menu = ::GetMenu(hDlg);
        if (menu == nullptr)
            return;
        ::CheckMenuRadioItem(menu, IDM_MODE_SYSTEM, IDM_MODE_LIGHT, g_modeId, MF_BYCOMMAND);
        ::CheckMenuItem(menu, IDM_HOOK_TOGGLE,
            MF_BYCOMMAND | (g_hookOn ? MF_CHECKED : MF_UNCHECKED));
    }

    void RepaintAll(HWND hDlg)
    {
        // Force a full repaint so controls that read sys colors on paint /
        // WM_CTLCOLOR* re-query through (or around) the hook.
        ::RedrawWindow(hDlg, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
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
            { STD_PROPERTIES, IDM_HELP_ABOUT,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { STD_FILENEW,    IDM_FILE_EXIT,   TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        };
        ::SendMessageW(g_hToolbar, TB_ADDBUTTONSW,
            static_cast<WPARAM>(ARRAYSIZE(buttons)), reinterpret_cast<LPARAM>(buttons));
        ::SendMessageW(g_hToolbar, TB_AUTOSIZE, 0, 0);

        g_hStatus = ::CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_STATUSBAR), g_hInst, nullptr);

        int parts[] = { 360, -1 };
        ::SendMessageW(g_hStatus, SB_SETPARTS, ARRAYSIZE(parts), reinterpret_cast<LPARAM>(parts));
        ::SendMessageW(g_hStatus, SB_SETTEXTW, 0,
            reinterpret_cast<LPARAM>(L"umbra-hook \x2014 toggle the hook in the Mode menu"));
    }

    void CreateTabAndPages(HWND hDlg)
    {
        g_hTab = ::CreateWindowExW(0, WC_TABCONTROLW, nullptr,
            WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_TABSTOP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_TAB), g_hInst, nullptr);

        if (HFONT f = reinterpret_cast<HFONT>(::SendMessageW(hDlg, WM_GETFONT, 0, 0)))
            ::SendMessageW(g_hTab, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);

        for (int i = 0; i < 4; ++i)
        {
            TCITEMW it{};
            it.mask = TCIF_TEXT;
            it.pszText = const_cast<LPWSTR>(kPages[i].label);
            ::SendMessageW(g_hTab, TCM_INSERTITEMW, i, reinterpret_cast<LPARAM>(&it));
            g_hPages[i] = ::CreateDialogParamW(g_hInst, MAKEINTRESOURCEW(kPages[i].templ),
                hDlg, kPages[i].proc, 0);
        }

        // Tab control was created before the pages, so push it behind them;
        // ShowPage() brings the active page to the front.
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
                L"Color-path probe for UMBRA's process-wide Detours hook.\n\n"
                L"No per-window theming is applied. Appearance comes only from two\n"
                L"process-wide hooks: GetSysColor/Brush (classic + DirectUI) and\n"
                L"uxtheme OpenThemeData (themed common controls, dark variants).",
                L"About umbra-hook", MB_OK | MB_ICONINFORMATION);
            return TRUE;

        // The Mode items pick which palette the hook serves (re-read live).
        case IDM_MODE_SYSTEM:
        case IDM_MODE_DARK:
        case IDM_MODE_LIGHT:
            ApplyPalette(static_cast<int>(LOWORD(wParam)));
            UpdateStatus();
            UpdateMenuChecks(hDlg);
            RepaintAll(hDlg);
            return TRUE;

        // Install / uninstall the process-wide GetSysColor(Brush) hook.
        case IDM_HOOK_TOGGLE:
            if (g_hookOn)
            {
                unsetProcessWideColorHook();
                g_hookOn = false;
            }
            else
            {
                g_hookOn = setProcessWideColorHook();
            }
            UpdateStatus();
            UpdateMenuChecks(hDlg);
            RepaintAll(hDlg);
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

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    StartCreateWindowLog();   // THROWAWAY diagnostic: log startup threads, CreateThread, and window->thread

    INITCOMMONCONTROLSEX icc{ sizeof(icc),
        ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES |
        ICC_LISTVIEW_CLASSES | ICC_TREEVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_UPDOWN_CLASS |
        ICC_DATE_CLASSES | ICC_HOTKEY_CLASS | ICC_INTERNET_CLASSES | ICC_LINK_CLASS |
        ICC_USEREX_CLASSES | ICC_COOL_CLASSES };
    ::InitCommonControlsEx(&icc);

    // Full umbra dark-mode init (experimental + app dark mode + palette), forced
    // dark — before any window or extra thread exists.
    umbra::initDarkMode();
    ApplyPalette(IDM_MODE_DARK);
    g_hookOn = setProcessWideColorHook();              // classic + DirectUI color residue
    g_themeColorOn = setProcessWideThemeColorHook();   // uxtheme GetThemeColor probe (DUI residue; logs themecolor.log)

    // Install auto-theming while this is still the ONLY thread: the current-thread
    // WH_CALLWNDPROCRET hook + a CreateThread detour together cover every thread
    // the process spawns afterward (COM workers, and the shell's property-sheet
    // thread that raises the Permissions / Advanced Security dialogs) — no sweep.
    g_themeOn = setAutoDarkMode();

    // COM for tab E's shell objects; any threads it spawns now inherit the hook.
    const HRESULT hrCo = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HWND hDlg = ::CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_FRAME),
        nullptr, FrameProc, 0);
    if (hDlg == nullptr)
    {
        unsetAutoDarkMode();
        unsetProcessWideThemeColorHook();
        unsetProcessWideColorHook();
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

    unsetAutoDarkMode();
    unsetProcessWideThemeColorHook();
    unsetProcessWideColorHook();
    StopCreateWindowLog();
    if (SUCCEEDED(hrCo)) ::CoUninitialize();
    return static_cast<int>(msg.wParam);
}
