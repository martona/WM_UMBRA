#pragma once
#include <windows.h>

// Shared across the umbra-hook translation units.

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

// --- Process-wide dark hooks (ProcessColorHook.cpp / AutoDarkMode.cpp) ------
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

// --- THROWAWAY: CreateWindowEx creation logger (createwlog.cpp) -------------
void StartCreateWindowLog();
void StopCreateWindowLog();
