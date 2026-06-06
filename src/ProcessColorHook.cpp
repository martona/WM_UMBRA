// SPDX-License-Identifier: MIT
//
// ProcessColorHook.cpp — process-wide GetSysColor / GetSysColorBrush hook.
//
// A parallel mechanism to UMBRA's per-window theming: instead of subclassing
// individual controls, this inline-hooks the user32 exports GetSysColor and
// GetSysColorBrush (via Microsoft Detours) so every caller in the process that
// goes through those exports receives UMBRA's dark palette values.
//
// Scope & caveats:
//  * Hooking the export only catches code that CALLS the export. user32's own
//    non-client painting and DefWindowProc fills read the shared sys-color
//    table directly and are unaffected; uxtheme/theme-part drawing gets colors
//    from theme parts, not these APIs.
//  * GetSysColor is context-free (no HWND), so this is necessarily all-or-
//    nothing per process: while installed it serves UMBRA's current palette.
//    Install/uninstall (set/unset) is the on/off switch; there is no separate
//    enabled flag, so the host controls effect purely by installing the hook.
//  * Independent of the comctl32 IAT hook in SysColorHook.cpp; the two coexist
//    (the IAT proc's fall-through to the real GetSysColor lands on this hook).
//
// Returned colors track UMBRA's live palette (the get*Color accessors), so the
// host must have initialised the palette (initDarkMode / setDarkModeConfig /
// setDefaultColors) for the values to be meaningful.

#include <windows.h>
#include <detours/detours.h>

#include <mutex>
#include <vector>

#include "umbra.h"

// Detours doesn't self-link; this records the dependency in umbra.lib so a
// consumer (which pulls detours transitively via vcpkg) links it too.
#pragma comment(lib, "detours.lib")

namespace
{
    using fnGetSysColor      = DWORD  (WINAPI*)(int);
    using fnGetSysColorBrush = HBRUSH (WINAPI*)(int);

    // Trampolines to the real exports; Detours rewrites these to point at the
    // relocated originals on attach. Seeded with the export so a stray call
    // before/after attach still routes correctly.
    fnGetSysColor      g_realGetSysColor      = ::GetSysColor;
    fnGetSysColorBrush g_realGetSysColorBrush = ::GetSysColorBrush;

    bool g_installed = false;

    // ---- index -> UMBRA dark color -----------------------------------------
    // Returns false for indices we don't override (caller then uses the real
    // function). Mapping mirrors how UMBRA seeds its palette from sys colors.
    bool MapDarkColor(int nIndex, COLORREF& out)
    {
        switch (nIndex)
        {
        // --- backgrounds ---
        case COLOR_WINDOW:                out = umbra::getCtrlBackgroundColor(); return true;
        case COLOR_BTNFACE:               // == COLOR_3DFACE
        case COLOR_3DLIGHT:
        case COLOR_ACTIVECAPTION:
        case COLOR_INACTIVECAPTION:
        case COLOR_MENU:
        case COLOR_MENUBAR:
        case COLOR_SCROLLBAR:
        case COLOR_BACKGROUND:            // == COLOR_DESKTOP
        case COLOR_APPWORKSPACE:
        case COLOR_INFOBK:                out = umbra::getBackgroundColor(); return true;

        // --- text ---
        case COLOR_WINDOWTEXT:
        case COLOR_BTNTEXT:
        case COLOR_MENUTEXT:
        case COLOR_CAPTIONTEXT:
        case COLOR_INFOTEXT:
        case COLOR_HIGHLIGHTTEXT:         out = umbra::getTextColor(); return true;
        case COLOR_GRAYTEXT:
        case COLOR_INACTIVECAPTIONTEXT:   out = umbra::getDisabledTextColor(); return true;
        case COLOR_HOTLIGHT:              out = umbra::getLinkTextColor(); return true;

        // --- selection / highlight ---
        case COLOR_HIGHLIGHT:
        case COLOR_MENUHILIGHT:           out = umbra::getHotBackgroundColor(); return true;

        // --- edges / 3D ---
        case COLOR_WINDOWFRAME:
        case COLOR_3DDKSHADOW:
        case COLOR_3DSHADOW:              // == COLOR_BTNSHADOW
        case COLOR_ACTIVEBORDER:
        case COLOR_INACTIVEBORDER:        out = umbra::getEdgeColor(); return true;
        case COLOR_3DHIGHLIGHT:           out = umbra::getHotEdgeColor(); return true; // == BTNHIGHLIGHT/3DHILIGHT

        default:                          return false;
        }
    }

    // ---- brush cache --------------------------------------------------------
    // GetSysColorBrush must return a stable, process-lifetime brush per index
    // (callers must never delete it). We cache one brush per index and rebuild
    // it when the mapped color changes (palette/mode switch), retiring the old
    // handle for deletion at unset rather than deleting it live.
    std::mutex          g_brushMutex;
    std::vector<HBRUSH> g_retiredBrushes;

    struct BrushEntry { HBRUSH hbr; COLORREF clr; bool valid; };
    BrushEntry g_brushCache[COLOR_MENUBAR + 1]{};   // indices 0..30

    HBRUSH CachedBrush(int nIndex, COLORREF clr)
    {
        if (nIndex < 0 || nIndex >= static_cast<int>(ARRAYSIZE(g_brushCache)))
            return ::CreateSolidBrush(clr);   // outside cache range (mapped indices are within)

        std::lock_guard<std::mutex> lock(g_brushMutex);
        BrushEntry& e = g_brushCache[nIndex];
        if (e.valid && e.clr == clr)
            return e.hbr;

        HBRUSH nb = ::CreateSolidBrush(clr);
        if (e.valid && e.hbr != nullptr)
            g_retiredBrushes.push_back(e.hbr);   // defer delete; a caller may still hold it
        e.hbr = nb;
        e.clr = clr;
        e.valid = true;
        return nb;
    }

    void DestroyBrushes()
    {
        std::lock_guard<std::mutex> lock(g_brushMutex);
        for (BrushEntry& e : g_brushCache)
        {
            if (e.hbr != nullptr)
                ::DeleteObject(e.hbr);
            e = BrushEntry{};
        }
        for (HBRUSH h : g_retiredBrushes)
            ::DeleteObject(h);
        g_retiredBrushes.clear();
    }

    // ---- hook bodies --------------------------------------------------------
    DWORD WINAPI HookGetSysColor(int nIndex)
    {
        COLORREF c{};
        if (MapDarkColor(nIndex, c))
            return c;
        return g_realGetSysColor(nIndex);     // trampoline, NOT ::GetSysColor (would self-recurse)
    }

    HBRUSH WINAPI HookGetSysColorBrush(int nIndex)
    {
        COLORREF c{};
        if (MapDarkColor(nIndex, c))
            return CachedBrush(nIndex, c);
        return g_realGetSysColorBrush(nIndex);
    }
}

namespace umbra
{
    bool setProcessWideColorHook() noexcept
    {
        if (g_installed)
            return true;

        if (::DetourTransactionBegin() != NO_ERROR)
            return false;
        ::DetourUpdateThread(::GetCurrentThread());

        LONG err = ::DetourAttach(reinterpret_cast<PVOID*>(&g_realGetSysColor),
                                  reinterpret_cast<PVOID>(HookGetSysColor));
        if (err == NO_ERROR)
            err = ::DetourAttach(reinterpret_cast<PVOID*>(&g_realGetSysColorBrush),
                                 reinterpret_cast<PVOID>(HookGetSysColorBrush));

        if (err != NO_ERROR)
        {
            ::DetourTransactionAbort();
            return false;
        }

        err = ::DetourTransactionCommit();
        g_installed = (err == NO_ERROR);
        return g_installed;
    }

    void unsetProcessWideColorHook() noexcept
    {
        if (!g_installed)
            return;

        if (::DetourTransactionBegin() == NO_ERROR)
        {
            ::DetourUpdateThread(::GetCurrentThread());
            ::DetourDetach(reinterpret_cast<PVOID*>(&g_realGetSysColor),
                           reinterpret_cast<PVOID>(HookGetSysColor));
            ::DetourDetach(reinterpret_cast<PVOID*>(&g_realGetSysColorBrush),
                           reinterpret_cast<PVOID>(HookGetSysColorBrush));
            ::DetourTransactionCommit();
        }

        g_installed = false;
        DestroyBrushes();
    }
}
