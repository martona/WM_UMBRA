// WM_UMBRA sample application
// ----------------------------
// A small classic Win32 app that demonstrates WM_UMBRA: a menu, toolbar,
// status bar, common controls, and a dark-mode-aware MessageBox, all of which
// follow the Windows app theme live (try Settings > Personalization > Colors,
// or the Mode menu).

// ===========================================================================
//  Enable the Common Controls v6 (themed) DLL via an activation context.
//  WITHOUT THIS, the toolbar / status bar / list view / tree view fall back to
//  the unthemed Windows-95 common controls and dark mode cannot style them.
//  We embed the dependency from source with a linker pragma instead of
//  shipping a separate .manifest file:
// ===========================================================================
#pragma comment(linker,                                 \
    "\"/manifestdependency:type='win32' "              \
    "name='Microsoft.Windows.Common-Controls' "         \
    "version='6.0.0.0' processorArchitecture='*' "      \
    "publicKeyToken='6595b64144ccf1df' language='*'\"")

#include <windows.h>
#include <shellapi.h>          // ShellExecuteW (excluded by WIN32_LEAN_AND_MEAN)
#include <commctrl.h>
#include <initializer_list>    // braced-list range-for
#include "umbra.h"
#include "resource.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")   // ShellExecuteW for the SysLink

namespace
{
    HINSTANCE g_hInst    = nullptr;
    HWND      g_hToolbar = nullptr;
    HWND      g_hStatus  = nullptr;
    int       g_modeId   = IDM_MODE_SYSTEM;  // active Mode item, for the radio check

    void UpdateModeStatus()
    {
        if (g_hStatus != nullptr)
        {
            ::SendMessageW(g_hStatus, SB_SETTEXTW, 1,
                reinterpret_cast<LPARAM>(umbra::isEnabled() ? L"Mode: Dark" : L"Mode: Light"));
        }
    }

    // Reflect the active mode + color tone as radio checks in the menus.
    void UpdateMenuChecks(HWND hDlg)
    {
        HMENU menu = ::GetMenu(hDlg);
        if (menu == nullptr)
            return;
        ::CheckMenuRadioItem(menu, IDM_MODE_SYSTEM, IDM_MODE_LIGHT, g_modeId, MF_BYCOMMAND);
        const int toneId = IDM_TONE_BLACK + static_cast<int>(umbra::getColorTone());
        ::CheckMenuRadioItem(menu, IDM_TONE_BLACK, IDM_TONE_OLIVE, toneId, MF_BYCOMMAND);
    }

    // Re-apply the dark/light theme to the whole window tree and repaint.
    void ReapplyTheme(HWND hDlg)
    {
        umbra::setDarkWndNotifySafe(hDlg);
        umbra::setDarkTitleBar(hDlg);
        UpdateModeStatus();
        UpdateMenuChecks(hDlg);
        ::DrawMenuBar(hDlg);   // repaint the (subclassed) menu bar with the new colors
        ::RedrawWindow(hDlg, nullptr, nullptr,
            RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME | RDW_UPDATENOW);
    }

    void CreateBars(HWND hDlg)
    {
        // --- Toolbar, using the standard system small-image set ---
        g_hToolbar = ::CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_TOOLBAR), g_hInst, nullptr);

        ::SendMessageW(g_hToolbar, TB_BUTTONSTRUCTSIZE, sizeof(TBBUTTON), 0);

        TBADDBITMAP tbab{ HINST_COMMCTRL, IDB_STD_SMALL_COLOR };
        ::SendMessageW(g_hToolbar, TB_ADDBITMAP, 0, reinterpret_cast<LPARAM>(&tbab));

        TBBUTTON buttons[] = {
            { STD_PROPERTIES, IDC_BTN_MSGBOX, TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
            { STD_FILEOPEN,   IDC_BTN,        TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
            { 0, 0, 0, BTNS_SEP, {0}, 0, 0 },
            { STD_FILENEW,    IDM_FILE_EXIT,  TBSTATE_ENABLED, BTNS_BUTTON, {0}, 0, 0 },
        };
        ::SendMessageW(g_hToolbar, TB_ADDBUTTONSW,
            static_cast<WPARAM>(ARRAYSIZE(buttons)), reinterpret_cast<LPARAM>(buttons));
        ::SendMessageW(g_hToolbar, TB_AUTOSIZE, 0, 0);

        // --- Status bar with two parts ---
        g_hStatus = ::CreateWindowExW(0, STATUSCLASSNAMEW, nullptr,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hDlg, reinterpret_cast<HMENU>(IDC_STATUSBAR), g_hInst, nullptr);

        int parts[] = { 220, -1 };
        ::SendMessageW(g_hStatus, SB_SETPARTS, ARRAYSIZE(parts), reinterpret_cast<LPARAM>(parts));
        ::SendMessageW(g_hStatus, SB_SETTEXTW, 0, reinterpret_cast<LPARAM>(L"Ready"));
    }

    void PopulateControls(HWND hDlg)
    {
        // Combo box
        HWND combo = ::GetDlgItem(hDlg, IDC_COMBO);
        for (const wchar_t* s : { L"First item", L"Second item", L"Third item" })
            ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        ::SendMessageW(combo, CB_SETCURSEL, 0, 0);

        ::SetDlgItemTextW(hDlg, IDC_EDIT, L"Editable text field");
        ::SetDlgItemTextW(hDlg, IDC_EDIT_MULTI,
            L"WM_UMBRA themes edit boxes,\r\nlists, trees, scroll bars and more.");

        // List view (report mode, two columns)
        HWND lv = ::GetDlgItem(hDlg, IDC_LIST);
        ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 96; col.pszText = const_cast<LPWSTR>(L"Control");
        ListView_InsertColumn(lv, 0, &col);
        col.cx = 50; col.pszText = const_cast<LPWSTR>(L"State");
        ListView_InsertColumn(lv, 1, &col);

        const wchar_t* rows[][2] = {
            { L"Button",   L"Themed" },
            { L"Edit",     L"Themed" },
            { L"ListView", L"Themed" },
        };
        for (int i = 0; i < ARRAYSIZE(rows); ++i)
        {
            LVITEMW it{};
            it.mask = LVIF_TEXT;
            it.iItem = i;
            it.pszText = const_cast<LPWSTR>(rows[i][0]);
            ListView_InsertItem(lv, &it);
            ListView_SetItemText(lv, i, 1, const_cast<LPWSTR>(rows[i][1]));
        }

        // Tree view
        HWND tv = ::GetDlgItem(hDlg, IDC_TREE);
        TVINSERTSTRUCTW tis{};
        tis.hParent = TVI_ROOT;
        tis.hInsertAfter = TVI_LAST;
        tis.item.mask = TVIF_TEXT;
        tis.item.pszText = const_cast<LPWSTR>(L"Theme colors");
        HTREEITEM root = reinterpret_cast<HTREEITEM>(
            ::SendMessageW(tv, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tis)));

        tis.hParent = root;
        for (const wchar_t* s : { L"Background", L"Text", L"Edges" })
        {
            tis.item.pszText = const_cast<LPWSTR>(s);
            ::SendMessageW(tv, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tis));
        }
        ::SendMessageW(tv, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(root));

        // Progress + trackbar
        HWND prog = ::GetDlgItem(hDlg, IDC_PROGRESS);
        ::SendMessageW(prog, PBM_SETRANGE32, 0, 100);
        ::SendMessageW(prog, PBM_SETPOS, 62, 0);

        HWND track = ::GetDlgItem(hDlg, IDC_TRACK);
        ::SendMessageW(track, TBM_SETRANGE, TRUE, MAKELPARAM(0, 10));
        ::SendMessageW(track, TBM_SETPOS, TRUE, 6);
    }
}

INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        CreateBars(hDlg);
        PopulateControls(hDlg);
        // One call themes the dialog, all of its child controls, the ctl-color
        // messages, custom draw, and the title bar.
        umbra::setDarkWndNotifySafe(hDlg);
        umbra::setWindowMenuBarSubclass(hDlg);   // dark menu bar: background, items, highlight, bottom line
        umbra::setDarkTitleBar(hDlg);
        UpdateModeStatus();
        UpdateMenuChecks(hDlg);
        ::SendMessageW(hDlg, WM_SIZE, 0, 0);   // dock the bars
        return TRUE;

    case WM_SIZE:
        if (g_hToolbar != nullptr) ::SendMessageW(g_hToolbar, TB_AUTOSIZE, 0, 0);
        if (g_hStatus  != nullptr) ::SendMessageW(g_hStatus,  WM_SIZE, 0, 0);
        return FALSE;

    case WM_SETTINGCHANGE:
        // The user toggled the OS light/dark setting: re-theme live.
        if (umbra::handleSettingChange(lParam))
            ReapplyTheme(hDlg);
        return FALSE;

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDC_BTN_MSGBOX:
            umbra::DarkMessageBox(hDlg,
                L"This MessageBox is painted dark by WM_UMBRA\n"
                L"via a thread-local CBT hook.",
                L"WM_UMBRA", MB_OK | MB_ICONINFORMATION);
            return TRUE;

        case IDC_BTN:
            umbra::DarkMessageBox(hDlg, L"Push button clicked.", L"WM_UMBRA",
                MB_OK | MB_ICONASTERISK);
            return TRUE;

        case IDM_HELP_ABOUT:
            umbra::DarkMessageBox(hDlg,
                L"WM_UMBRA\n"
                L"Native dark mode for classic Win32 applications.\n\n"
                L"MIT licensed.",
                L"About WM_UMBRA", MB_OK | MB_ICONINFORMATION);
            return TRUE;

        case IDM_MODE_SYSTEM:
            g_modeId = IDM_MODE_SYSTEM;
            umbra::setDarkModeConfig();
            umbra::setDefaultColors(true);
            ReapplyTheme(hDlg);
            return TRUE;

        case IDM_MODE_DARK:
            g_modeId = IDM_MODE_DARK;
            umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::dark));
            umbra::setDefaultColors(true);
            ReapplyTheme(hDlg);
            return TRUE;

        case IDM_MODE_LIGHT:
            g_modeId = IDM_MODE_LIGHT;
            umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::classic));
            umbra::setDefaultColors(true);
            ReapplyTheme(hDlg);
            return TRUE;

        case IDM_TONE_BLACK:
        case IDM_TONE_RED:
        case IDM_TONE_GREEN:
        case IDM_TONE_BLUE:
        case IDM_TONE_PURPLE:
        case IDM_TONE_CYAN:
        case IDM_TONE_OLIVE:
            // Tones are dark-mode accent palettes, so picking one selects dark mode.
            umbra::setColorTone(static_cast<umbra::ColorTone>(LOWORD(wParam) - IDM_TONE_BLACK));
            g_modeId = IDM_MODE_DARK;
            umbra::setDarkModeConfig(static_cast<UINT>(umbra::DarkModeType::dark));
            umbra::setDefaultColors(true);
            // List/tree views use a separate, fixed view palette; point it at the tone's
            // background so they follow the accent instead of staying default-dark.
            umbra::setViewBackgroundColor(umbra::getBackgroundColor());
            umbra::setViewTextColor(umbra::getTextColor());
            umbra::updateViewBrushesAndPens();
            ReapplyTheme(hDlg);
            return TRUE;

        case IDM_FILE_EXIT:
        case IDCANCEL:
            ::DestroyWindow(hDlg);
            return TRUE;
        }
        return FALSE;

    case WM_NOTIFY:
    {
        auto* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr->idFrom == IDC_SYSLINK &&
            (nmhdr->code == NM_CLICK || nmhdr->code == NM_RETURN))
        {
            auto* link = reinterpret_cast<PNMLINK>(lParam);
            ::ShellExecuteW(hDlg, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        return FALSE;
    }

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

    INITCOMMONCONTROLSEX icc{ sizeof(icc),
        ICC_WIN95_CLASSES | ICC_LINK_CLASS | ICC_STANDARD_CLASSES };
    ::InitCommonControlsEx(&icc);

    // One-time dark mode init; follows the current Windows app theme.
    umbra::initDarkMode();

    HWND hDlg = ::CreateDialogParamW(hInstance, MAKEINTRESOURCEW(IDD_MAIN),
        nullptr, MainDlgProc, 0);
    if (hDlg == nullptr)
        return 1;

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
    return static_cast<int>(msg.wParam);
}
