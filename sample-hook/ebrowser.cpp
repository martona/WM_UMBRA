// umbra-hook — Tab E: DirectUI surfaces.
//
//   * an embedded IExplorerBrowser hosting the real Explorer view (This PC) —
//     the authentic DUI70 items view, live in our window;
//   * a button that pops IFileOpenDialog (also DirectUI);
//   * a button that pops the classic comdlg32 ChooseColor dialog, which is a
//     source-A surface, shown for contrast.
//
// The browser is created once and kept alive for the app's lifetime (recreating
// a shell view on every tab switch is just more lifetime management), and torn
// down from the page's WM_DESTROY before CoUninitialize runs.

#include <windows.h>
#include <shlobj.h>            // IExplorerBrowser, SHGetSpecialFolderLocation, CSIDL_DRIVES
#include <shobjidl.h>          // IFileOpenDialog, CLSID_FileOpenDialog
#include <commdlg.h>           // ChooseColor (comdlg32, source A)
#include "hook.h"
#include "resource.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "uuid.lib")

namespace
{
    IExplorerBrowser* g_peb = nullptr;

    // The browser fills the page below the host label.
    RECT BrowserRect(HWND page)
    {
        RECT rc;
        ::GetClientRect(page, &rc);
        int top = 60;
        if (HWND lbl = ::GetDlgItem(page, IDC_E_HOSTLBL))
        {
            RECT lr;
            ::GetWindowRect(lbl, &lr);
            ::MapWindowPoints(nullptr, page, reinterpret_cast<POINT*>(&lr), 2);
            top = lr.bottom + 4;
        }
        RECT v{ 4, top, rc.right - 4, rc.bottom - 4 };
        if (v.bottom < v.top) v.bottom = v.top;
        return v;
    }
}

bool ExplorerBrowserCreate(HWND page)
{
    if (g_peb != nullptr)
        return true;

    if (FAILED(::CoCreateInstance(CLSID_ExplorerBrowser, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&g_peb))) || g_peb == nullptr)
        return false;

    RECT rc = BrowserRect(page);
    FOLDERSETTINGS fs{};
    fs.ViewMode = FVM_DETAILS;
    fs.fFlags   = 0;
    if (FAILED(g_peb->Initialize(page, &rc, &fs)))
    {
        g_peb->Release();
        g_peb = nullptr;
        return false;
    }

    // Browse to "This PC" (the classic My Computer / drives folder).
    LPITEMIDLIST pidl = nullptr;
    if (SUCCEEDED(::SHGetSpecialFolderLocation(nullptr, CSIDL_DRIVES, &pidl)) && pidl != nullptr)
    {
        g_peb->BrowseToIDList(reinterpret_cast<PCIDLIST_ABSOLUTE>(pidl), SBSP_ABSOLUTE);
        ::CoTaskMemFree(pidl);
    }
    return true;
}

void ExplorerBrowserLayout(HWND page)
{
    if (g_peb == nullptr)
        return;
    RECT v = BrowserRect(page);
    g_peb->SetRect(nullptr, v);
}

void ExplorerBrowserDestroy()
{
    if (g_peb != nullptr)
    {
        g_peb->Destroy();
        g_peb->Release();
        g_peb = nullptr;
    }
}

void ShowModernFileDialog(HWND owner)
{
    IFileOpenDialog* dlg = nullptr;
    if (SUCCEEDED(::CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&dlg))) && dlg != nullptr)
    {
        dlg->Show(owner);
        dlg->Release();
    }
}

void ShowClassicColorDialog(HWND owner)
{
    static COLORREF custom[16]{};
    CHOOSECOLORW cc{};
    cc.lStructSize  = sizeof(cc);
    cc.hwndOwner    = owner;
    cc.lpCustColors = custom;
    cc.rgbResult    = RGB(0, 120, 215);
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    ::ChooseColorW(&cc);
}

void ShowPermissionsDialog(HWND owner)
{
    // The standard Win32 file Security dialog (Explorer's Properties > Security).
    // It's a comctl32 property sheet, but its "Advanced" button opens the
    // Advanced Security Settings editor — a DirectUI (DUI70) surface. We point it
    // at our own .exe, which always exists.
    wchar_t path[MAX_PATH];
    if (::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path)) > 0)
        ::SHObjectProperties(owner, SHOP_FILEPATH, path, L"Security");
}
