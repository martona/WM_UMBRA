# WM_UMBRA — vcpkg overlay port

Consume WM_UMBRA through vcpkg today, without waiting on the public registry.
The port here builds the `umbra` static library from the tagged release.

## Manifest mode (recommended)

In your project's `vcpkg-configuration.json`:

```json
{
  "overlay-ports": [ "path/to/WM_UMBRA/vcpkg-overlay" ]
}
```

and in your `vcpkg.json`:

```json
{ "dependencies": [ "umbra" ] }
```

## Classic mode

```powershell
vcpkg install umbra --overlay-ports=path/to/WM_UMBRA/vcpkg-overlay
```

## Use it from CMake

```cmake
find_package(umbra CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE umbra::umbra)
```

`umbra` is a **static library**; its CRT model follows the triplet you build
(`x64-windows` → dynamic CRT, `x64-windows-static` → static CRT), so it always
matches your application. Windows desktop only (`windows & !uwp`).

To track the latest commit instead of the tagged release, add `--head`.
