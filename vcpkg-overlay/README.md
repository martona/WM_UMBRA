# WM_UMBRA — using it from vcpkg

WM_UMBRA ships the `umbra` static library as a vcpkg port you can consume two
ways. The port recipe is a single source in [`umbra/`](umbra); the repo also
publishes a versions database at [`../versions/`](../versions), which makes it a
self-hosted **git registry** — so you can consume it straight from the URL with
no local clone.

## Option 1 — git registry (by URL) ✅ recommended

In your project's `vcpkg-configuration.json`:

```json
{
  "default-registry": {
    "kind": "git",
    "repository": "https://github.com/microsoft/vcpkg",
    "baseline": "<your usual vcpkg baseline>"
  },
  "registries": [
    {
      "kind": "git",
      "repository": "https://github.com/martona/WM_UMBRA",
      "baseline": "e87d7a27e70da5d9e144622bc27c3c0a85336e36",
      "packages": [ "umbra" ]
    }
  ]
}
```

and depend on it in `vcpkg.json`:

```json
{ "dependencies": [ "umbra" ] }
```

The registry `baseline` is a commit SHA of this repo's `main` (it selects which
versions are visible); the value above ships **v1.1.0**. Grab the latest with:

```powershell
git ls-remote https://github.com/martona/WM_UMBRA.git main
```

or run `vcpkg x-update-baseline` to refresh it automatically. Pin an older
commit to hold a specific WM_UMBRA version.

## Option 2 — overlay port (local checkout)

If you already have this repo on disk, skip the registry and point vcpkg at the
port directory:

```powershell
# Manifest mode: add to vcpkg-configuration.json
#   { "overlay-ports": [ "path/to/WM_UMBRA/vcpkg-overlay" ] }
# and list "umbra" in vcpkg.json. Or classic mode:
vcpkg install umbra --overlay-ports=path/to/WM_UMBRA/vcpkg-overlay
```

`overlay-ports` only accepts a **local path** — that's why the registry above
exists for URL-based consumption.

## Then, in CMake

```cmake
find_package(umbra CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE umbra::umbra)
```

`umbra` is a **static library**; its CRT model follows your triplet
(`x64-windows` → dynamic CRT, `x64-windows-static` → static CRT), so it always
matches your application. Windows desktop only (`windows & !uwp`).

## Maintainers — cutting a release

Use [`../bump_version.ps1`](../bump_version.ps1) (run `Get-Help .\bump_version.ps1 -Detailed`).
The per-release flow keeps `Version.h`, `CMakeLists.txt`, `vcpkg.json`, the
portfile hash, and the versions DB all in lockstep:

```powershell
.\bump_version.ps1 1.2.0                       # 1. bump Version.h / CMakeLists / vcpkg.json
git commit -am "Bump version to 1.2.0"
git tag -a v1.2.0 -m "WM_UMBRA v1.2.0"; git push origin main v1.2.0

.\bump_version.ps1 1.2.0 -UpdateHash           # 2. portfile SHA512 from the pushed tag
git commit -am "vcpkg: SHA512 for v1.2.0"

.\bump_version.ps1 1.2.0 -AddVersion           # 3. register the committed port in versions/
git commit -am "vcpkg: register umbra 1.2.0"; git push origin main
```

Consumers then bump their registry `baseline` to the new commit.
