#pragma once
#include <windows.h>
#include <strsafe.h>

// Shared across the umbra-hook translation units.

// --- Diagnostic logs: one fixed location, one file per target -------------
// Every harness/payload diagnostic log goes to ONE hardcoded directory under the repo, so
// they never scatter across module dirs. (The old <module dir> path resolved to C:\Windows /
// system32 for an injected regedit/mmc/explorer — which a medium-integrity process can't even
// write — so logs silently vanished or landed somewhere you had to hunt for. A repo path under
// the user profile is writable by both elevated and medium-integrity targets.) The running
// .exe's name is appended to each log's base, so the global hook's several targets write
// SEPARATE files — themecolor-regedit.log, themecolor-mmc.log, umbra-inject-explorer.log —
// instead of clobbering one shared file. Hardcoded by design: throwaway diagnostic plumbing
// that never ships. Delete the files for a clean run.
inline constexpr const wchar_t* kUmbraLogDir =
    L"C:\\Users\\Marton\\Desktop\\github\\WM_UMBRA\\logs";

// Writes the running module's base name without extension into `out` (e.g. "regedit" for
// C:\Windows\regedit.exe; "umbra-hook" for the in-process harness). Falls back to "unknown".
inline void umbraExeStem(wchar_t* out, size_t outCount) noexcept
{
    wchar_t path[MAX_PATH];
    const DWORD n = ::GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (n == 0 || n >= ARRAYSIZE(path))
    {
        ::StringCchCopyW(out, outCount, L"unknown");
        return;
    }
    wchar_t* base = path;
    for (wchar_t* p = path; *p != L'\0'; ++p)
        if (*p == L'\\' || *p == L'/')
            base = p + 1;
    wchar_t* dot = nullptr;
    for (wchar_t* p = base; *p != L'\0'; ++p)
        if (*p == L'.')
            dot = p;
    if (dot != nullptr)
        *dot = L'\0';
    ::StringCchCopyW(out, outCount, base);
}

// Composes "<kUmbraLogDir>\<base>-<exe><ext>" into `out` (e.g. "...\themecolor-regedit.log"),
// creating the directory if needed. `out` is always left a valid (possibly truncated) string;
// returns false only if too small.
inline bool umbraLogPath(const wchar_t* fileName, wchar_t* out, size_t outCount) noexcept
{
    (void)::CreateDirectoryW(kUmbraLogDir, nullptr);   // succeeds, or already exists

    wchar_t exe[64];
    umbraExeStem(exe, ARRAYSIZE(exe));

    // Split fileName at its last '.', so the exe stem lands on the base, before the extension:
    // "themecolor.log" -> "themecolor-<exe>.log".
    wchar_t base[64];
    const wchar_t* ext = L"";
    const wchar_t* dot = nullptr;
    for (const wchar_t* p = fileName; *p != L'\0'; ++p)
        if (*p == L'.')
            dot = p;
    if (dot != nullptr)
    {
        ext = dot;
        ::StringCchCopyNW(base, ARRAYSIZE(base), fileName, static_cast<size_t>(dot - fileName));
    }
    else
    {
        ::StringCchCopyW(base, ARRAYSIZE(base), fileName);
    }

    return SUCCEEDED(::StringCchPrintfW(out, outCount, L"%s\\%s-%s%s",
                                        kUmbraLogDir, base, exe, ext));
}

extern HINSTANCE g_hInst;

// --- Page dialog procedures (pages.cpp) ------------------------------------
INT_PTR CALLBACK PageAProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PageBProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PageCProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PageEProc(HWND, UINT, WPARAM, LPARAM);

// --- Tab B: export-vs-internal specimen panels (bpanels.cpp) ---------------
// Two child windows that fill their background two different ways, so a future
// GetSysColor(Brush) hook makes the export-vs-internal boundary visible:
//   - "export"   fills via GetSysColorBrush(COLOR_WINDOW)  -> reachable
//   - "internal" relies on the class background brush       -> user32-internal
void CreateBPanels(HWND page);
void LayoutBPanels(HWND page);

// --- Tab E: DirectUI surfaces (ebrowser.cpp) -------------------------------
bool ExplorerBrowserCreate(HWND page);   // embeds a live Explorer view (This PC)
void ExplorerBrowserLayout(HWND page);
void ExplorerBrowserDestroy();
void ShowModernFileDialog(HWND owner);   // IFileOpenDialog  (DirectUI)
void ShowClassicColorDialog(HWND owner); // ChooseColor      (comdlg32, source A)
void ShowPermissionsDialog(HWND owner);  // file Security dialog; its Advanced button = DirectUI ACE editor

// --- Process-wide dark hooks (ProcessColorHook / AutoDarkMode / ThemeColorHook) ---
// The application / interception half of UMBRA's dark mode: these install the
// Detours-based, process-wide hooks that drive the library's per-window and
// per-color theming decisions (umbra::applyDarkToNewWindow / umbra::darkSysColor).
// The Detours dependency lives here, in the app, not in the umbra library.

// GetSysColor / GetSysColorBrush inline hook (classic + DirectUI color residue).
bool setProcessWideColorHook() noexcept;
void unsetProcessWideColorHook() noexcept;

// WH_CALLWNDPROCRET + CreateThread self-hook: themes every window on every thread
// as it is created. Install before the window tree (and before CoInitializeEx).
bool setAutoDarkMode() noexcept;
void unsetAutoDarkMode() noexcept;

// uxtheme GetThemeColor probe (Tier 1, phase 1): maps HTHEME->class and logs every
// theme-colour query to themecolor.log. Pass-through — appearance is unchanged.
bool setProcessWideThemeColorHook() noexcept;
void unsetProcessWideThemeColorHook() noexcept;

// --- THROWAWAY: CreateWindowEx creation logger (createwlog.cpp) -------------
void StartCreateWindowLog();
void StopCreateWindowLog();
