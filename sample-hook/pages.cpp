// umbra-hook — tab page dialog procedures.
//   A: raw user32/GDI controls   C: comctl32/uxtheme controls
//   B: delegates to bpanels.cpp  E: delegates to ebrowser.cpp

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>          // ShellExecuteW for the SysLink
#include <initializer_list>
#include "hook.h"
#include "resource.h"

#pragma comment(lib, "shell32.lib")

// ---------------------------------------------------------------------------
// Tab A — raw user32 / GDI controls
// ---------------------------------------------------------------------------
INT_PTR CALLBACK PageAProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND combo = ::GetDlgItem(hDlg, IDC_A_COMBO);
        for (const wchar_t* s : { L"First item", L"Second item", L"Third item" })
            ::SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        ::SendMessageW(combo, CB_SETCURSEL, 0, 0);

        ::SetDlgItemTextW(hDlg, IDC_A_EDIT, L"Editable text field");
        ::SetDlgItemTextW(hDlg, IDC_A_EDIT_MULTI,
            L"Select this text to see the\r\nCOLOR_HIGHLIGHT selection color.");

        HWND lb = ::GetDlgItem(hDlg, IDC_A_LIST);
        for (const wchar_t* s : { L"List item 1", L"List item 2", L"List item 3",
                                  L"List item 4", L"List item 5", L"List item 6" })
            ::SendMessageW(lb, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));

        HWND sbar = ::GetDlgItem(hDlg, IDC_A_SCROLL);
        ::SetScrollRange(sbar, SB_CTL, 0, 100, FALSE);
        ::SetScrollPos(sbar, SB_CTL, 40, TRUE);
        return TRUE;
    }

    case WM_HSCROLL:
    {
        HWND sbar = ::GetDlgItem(hDlg, IDC_A_SCROLL);
        if (reinterpret_cast<HWND>(lParam) == sbar)
        {
            int pos = ::GetScrollPos(sbar, SB_CTL);
            switch (LOWORD(wParam))
            {
            case SB_LINELEFT:    --pos; break;
            case SB_LINERIGHT:   ++pos; break;
            case SB_PAGELEFT:    pos -= 10; break;
            case SB_PAGERIGHT:   pos += 10; break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: pos = HIWORD(wParam); break;
            default: break;
            }
            if (pos < 0)   pos = 0;
            if (pos > 100) pos = 100;
            ::SetScrollPos(sbar, SB_CTL, pos, TRUE);
        }
        return TRUE;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_A_BTN)
        {
            ::MessageBoxW(hDlg, L"Push button clicked.", L"umbra-hook",
                MB_OK | MB_ICONASTERISK);
            return TRUE;
        }
        return FALSE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Tab B — export-vs-internal exhibit (panels live in bpanels.cpp)
// ---------------------------------------------------------------------------
INT_PTR CALLBACK PageBProc(HWND hDlg, UINT msg, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        CreateBPanels(hDlg);
        return TRUE;
    case WM_SIZE:
        LayoutBPanels(hDlg);
        return FALSE;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Tab C — comctl32 over uxtheme
// ---------------------------------------------------------------------------
INT_PTR CALLBACK PageCProc(HWND hDlg, UINT msg, WPARAM /*wParam*/, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // List view (report mode, two columns, gridlines)
        HWND lv = ::GetDlgItem(hDlg, IDC_C_LIST);
        ListView_SetExtendedListViewStyle(lv, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
        LVCOLUMNW col{};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 92;  col.pszText = const_cast<LPWSTR>(L"Item");
        ListView_InsertColumn(lv, 0, &col);
        col.cx = 50;  col.pszText = const_cast<LPWSTR>(L"Value");
        ListView_InsertColumn(lv, 1, &col);

        const wchar_t* rows[][2] = {
            { L"Alpha", L"1" }, { L"Beta", L"2" }, { L"Gamma", L"3" }, { L"Delta", L"4" },
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
        HWND tv = ::GetDlgItem(hDlg, IDC_C_TREE);
        TVINSERTSTRUCTW tis{};
        tis.hParent = TVI_ROOT;
        tis.hInsertAfter = TVI_LAST;
        tis.item.mask = TVIF_TEXT;
        tis.item.pszText = const_cast<LPWSTR>(L"Root");
        HTREEITEM root = reinterpret_cast<HTREEITEM>(
            ::SendMessageW(tv, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tis)));
        tis.hParent = root;
        for (const wchar_t* s : { L"Child 1", L"Child 2", L"Child 3" })
        {
            tis.item.pszText = const_cast<LPWSTR>(s);
            ::SendMessageW(tv, TVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tis));
        }
        ::SendMessageW(tv, TVM_EXPAND, TVE_EXPAND, reinterpret_cast<LPARAM>(root));

        // Progress + trackbar
        HWND prog = ::GetDlgItem(hDlg, IDC_C_PROGRESS);
        ::SendMessageW(prog, PBM_SETRANGE32, 0, 100);
        ::SendMessageW(prog, PBM_SETPOS, 62, 0);
        HWND track = ::GetDlgItem(hDlg, IDC_C_TRACK);
        ::SendMessageW(track, TBM_SETRANGE, TRUE, MAKELPARAM(0, 10));
        ::SendMessageW(track, TBM_SETPOS, TRUE, 6);

        // Up-down with its buddy edit
        HWND spin = ::GetDlgItem(hDlg, IDC_C_SPIN);
        ::SendMessageW(spin, UDM_SETBUDDY,
            reinterpret_cast<WPARAM>(::GetDlgItem(hDlg, IDC_C_SPINBUDDY)), 0);
        ::SendMessageW(spin, UDM_SETRANGE32, 0, 100);
        ::SendMessageW(spin, UDM_SETPOS32, 0, 50);
        return TRUE;
    }

    case WM_NOTIFY:
    {
        auto* nmhdr = reinterpret_cast<LPNMHDR>(lParam);
        if (nmhdr->idFrom == IDC_C_SYSLINK &&
            (nmhdr->code == NM_CLICK || nmhdr->code == NM_RETURN))
        {
            auto* link = reinterpret_cast<PNMLINK>(lParam);
            ::ShellExecuteW(hDlg, L"open", link->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
            return TRUE;
        }
        return FALSE;
    }
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Tab E — DirectUI (implementation in ebrowser.cpp)
// ---------------------------------------------------------------------------
INT_PTR CALLBACK PageEProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        ExplorerBrowserCreate(hDlg);
        return TRUE;
    case WM_SIZE:
        ExplorerBrowserLayout(hDlg);
        return FALSE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_E_BTN_FILEDLG)  { ShowModernFileDialog(hDlg);  return TRUE; }
        if (LOWORD(wParam) == IDC_E_BTN_PERMS)    { ShowPermissionsDialog(hDlg); return TRUE; }
        if (LOWORD(wParam) == IDC_E_BTN_COLORDLG) { ShowClassicColorDialog(hDlg); return TRUE; }
        return FALSE;
    case WM_DESTROY:
        ExplorerBrowserDestroy();
        return FALSE;
    }
    return FALSE;
}
