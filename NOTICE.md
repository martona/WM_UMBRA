# WM_UMBRA — notices and provenance

**WM_UMBRA** is released under the [MIT License](./LICENSE.md), © 2026
Marton Anka. It is a fork of **darkmode32plus** by Anthony Lee Stark, which
itself incorporates code from several upstream projects. This file is the
attribution and provenance ledger: it does not narrow or replace the notices
in the individual source files or in [licenses/](./licenses).

Most of the inherited library code remains under the **BSD-3-Clause** license
by Anthony Lee Stark — that license is permissive and MIT-compatible, but it
cannot be removed: those portions stay BSD-3-Clause and their notice is
retained below. New WM_UMBRA code (the sample, the build system, and the
files marked below) is MIT.

## Provenance

- Upstream project: `darkmode32plus` — https://github.com/anthonyleestark/darkmode32plus
- Reviewed upstream baseline: commit `03ca2e956df57af4eadeec9076bb44ac596a9572` (2026-03-12)
- Fork: Marton Anka — https://github.com/martona/WM_UMBRA

Changes made in the fork: a library-breaking Windows build-number initialization 
fix; a dark-mode-aware `MessageBoxW` helper; removal of the `.ini` configuration 
code (which also fixed a mis-initialization that forced dark mode on regardless of
the system setting); a rename of the public namespace to `umbra` and the main
header to `umbra.h`; a sample application; and a CMake build with presets.

## File attribution

| File(s) | Origin / copyright | License |
| --- | --- | --- |
| `umbra.h`, `umbra.cpp` (formerly `DMSubclass.*`) | Anthony Lee Stark, 2025. Source comments identify Notepad++ dark-mode code by Adam D. Walling (`adzm`), with modifications by ozone10 and the Notepad++ team. Local fixes by Marton Anka, 2026. | BSD-3-Clause (the Notepad++-derived code was relicensed to BSD-3-Clause with permission, per the source headers). |
| `DarkMode.h`, `DarkMode.cpp`, `SysColorHook.h`, `SysColorHook.cpp`, `ModuleHelper.h`, `WinVerHelper.h`, `Version.h` | Anthony Lee Stark, 2025. Incorporates `win32-darkmode` by ysc3839 / Richard Yu (MIT) and `darkmodelib` by ozone10 (dual MPL-2.0 / MIT — **MIT elected** for this distribution). | BSD-3-Clause for the darkmode32plus portions; MIT for the incorporated portions. |
| `IatHook.h` | Anthony Lee Stark, 2025. Includes modified code from PolyHook 2.0 by Stephen Eckels (`stevemk14ebr`). | BSD-3-Clause; MIT for the PolyHook 2.0 portions. |
| `UAHMenuBar.h` | Anthony Lee Stark, 2025. Original UAH menu-bar code by Adam D. Walling (`adzm`), 2021. | BSD-3-Clause; MIT for the UAHMenuBar portions. |
| `DarkMessageBox.cpp` | Marton Anka, 2026. | MIT. |
| `sample/*`, `CMakeLists.txt`, `CMakePresets.json`, `cmake/*`, `umbra.sln`, `project/*` | Marton Anka / WM_UMBRA, 2026. | MIT. |

## Third-party projects

- `darkmode32plus` — https://github.com/anthonyleestark/darkmode32plus (BSD-3-Clause)
- `win32-darkmode` — https://github.com/ysc3839/win32-darkmode (MIT)
- `darkmodelib` — https://github.com/ozone10/darkmodelib (MPL-2.0 or MIT; MIT elected)
- Notepad++ — https://github.com/notepad-plus-plus/notepad-plus-plus (dark-mode code relicensed to BSD-3-Clause with permission)
- PolyHook 2.0 — https://github.com/stevemk14ebr/PolyHook_2_0 (MIT)
- UAHMenuBar by Adam D. Walling (`adzm`) — https://gist.github.com/adzm/2f82b2b5c7a3c6f007397e5ed885d0a6 (MIT)

Full license texts for the incorporated projects are in [licenses/](./licenses).

## Retained BSD-3-Clause notice (darkmode32plus)

```
BSD 3-Clause License

Copyright (c) 2025 Anthony Lee Stark
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of Anthony Lee Stark (@anthonyleestark) nor the names of
   its contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```
