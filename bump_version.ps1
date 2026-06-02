#requires -Version 5.1
<#
.SYNOPSIS
    Bump the WM_UMBRA version everywhere it lives, refresh the vcpkg portfile's
    source SHA512 from the pushed tag, and register the release in the vcpkg
    versions database.

.DESCRIPTION
    The version is kept in three places:
      * include/Version.h                 (DM_VERSION_* macros + version strings)
      * CMakeLists.txt                    (project(... VERSION ...) -> find_package version)
      * vcpkg-overlay/umbra/vcpkg.json    (version-semver)

    WM_UMBRA is also its own vcpkg git registry: vcpkg-overlay/umbra/ holds the
    single port recipe, and versions/ records each release's git-tree so the
    repo can be consumed straight from its URL. The portfile pins the SHA512 of
    the GitHub release tarball (which exists only after the tag is pushed), and
    the versions DB pins the port's *committed* git-tree (so the port must be
    committed before it is registered) -- hence the staged flow below.

.PARAMETER Version
    Target version as X.Y.Z (e.g. 1.2.0).

.PARAMETER UpdateHash
    Phase 2: download the v<Version> source tarball and write its SHA512 into
    vcpkg-overlay/umbra/portfile.cmake. Run after the tag is on GitHub.

.PARAMETER AddVersion
    Phase 3: register the (already-committed) port in versions/ via
    'vcpkg x-add-version'. Run after committing the portfile SHA512 update.

.EXAMPLE
    # Phase 1 - bump sources, tag & push
    .\bump_version.ps1 1.2.0
    git commit -am "Bump version to 1.2.0"
    git tag -a v1.2.0 -m "WM_UMBRA v1.2.0"
    git push origin main v1.2.0

    # Phase 2 - lock the portfile hash (after the tag is on GitHub)
    .\bump_version.ps1 1.2.0 -UpdateHash
    git commit -am "vcpkg: SHA512 for v1.2.0"

    # Phase 3 - register in the versions DB (after the portfile is committed)
    .\bump_version.ps1 1.2.0 -AddVersion
    git commit -am "vcpkg: register umbra 1.2.0"
    git push origin main
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory, Position = 0)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,

    [switch] $UpdateHash,
    [switch] $AddVersion,

    [string] $Repo = 'martona/WM_UMBRA'
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$maj, $min, $pat = $Version.Split('.')

# Rewrite a file's text in place, preserving its UTF-8 BOM (or absence thereof).
function Edit-File([string] $Rel, [scriptblock] $Transform) {
    $path  = Join-Path $root $Rel
    $bytes = [System.IO.File]::ReadAllBytes($path)
    $bom   = ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF)
    $text  = [System.IO.File]::ReadAllText($path)
    $new   = & $Transform $text
    if ($new -cne $text) {
        [System.IO.File]::WriteAllText($path, $new, (New-Object System.Text.UTF8Encoding($bom)))
        Write-Host "  updated  $Rel"
    }
    else {
        Write-Host "  current  $Rel"
    }
}

# Locate a vcpkg.exe (env var, PATH, or the Visual Studio-bundled copy).
function Get-Vcpkg {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT 'vcpkg.exe'))) {
        return (Join-Path $env:VCPKG_ROOT 'vcpkg.exe')
    }
    $cmd = Get-Command vcpkg -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vs = (& $vswhere -latest -property installationPath) | Select-Object -First 1
        if ($vs -and (Test-Path "$vs\VC\vcpkg\vcpkg.exe")) { return "$vs\VC\vcpkg\vcpkg.exe" }
    }
    throw "vcpkg not found. Set VCPKG_ROOT, put vcpkg on PATH, or install the Visual Studio C++/vcpkg component."
}

if ($AddVersion) {
    # ---- Phase 3: register the committed port in the versions database ----
    $vcpkg = Get-Vcpkg
    Write-Host "Registering umbra $Version in versions/ (vcpkg x-add-version)" -ForegroundColor Cyan
    & $vcpkg x-add-version umbra `
        --x-builtin-ports-root="$root\vcpkg-overlay" `
        --x-builtin-registry-versions-dir="$root\versions"
    if ($LASTEXITCODE -ne 0) {
        throw "x-add-version failed - is vcpkg-overlay/umbra committed with a clean working tree?"
    }
    Write-Host ""
    Write-Host "Next:" -ForegroundColor Cyan
    Write-Host "  git commit -am `"vcpkg: register umbra $Version`""
    Write-Host "  git push origin main"
}
elseif ($UpdateHash) {
    # ---- Phase 2: portfile SHA512 from the pushed tag ----
    $url = "https://github.com/$Repo/archive/v$Version.tar.gz"
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) "wm_umbra-v$Version.tar.gz"
    Write-Host "Fetching $url" -ForegroundColor Cyan

    $sha = $null
    for ($i = 0; $i -lt 6 -and -not $sha; $i++) {
        try {
            Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing -ErrorAction Stop
            $sha = (Get-FileHash $tmp -Algorithm SHA512).Hash.ToLower()
        }
        catch { Start-Sleep -Seconds 2 }
    }
    if (-not $sha) { throw "Could not download $url - is the v$Version tag pushed yet?" }

    Edit-File 'vcpkg-overlay/umbra/portfile.cmake' {
        param($t)
        $t -creplace '(SHA512\s+)[0-9a-fA-F]{128}', "`${1}$sha"
    }
    Write-Host "  SHA512 = $sha"
    Write-Host ""
    Write-Host "Next:" -ForegroundColor Cyan
    Write-Host "  git commit -am `"vcpkg: SHA512 for v$Version`""
    Write-Host "  .\bump_version.ps1 $Version -AddVersion    # then register in the versions DB"
}
else {
    # ---- Phase 1: bump the version everywhere ----
    Write-Host "Bumping WM_UMBRA -> $Version" -ForegroundColor Cyan

    Edit-File 'include/Version.h' {
        param($t)
        $t = $t -creplace '(#define\s+DM_VERSION_MAJOR\s+)\d+', "`${1}$maj"
        $t = $t -creplace '(#define\s+DM_VERSION_MINOR\s+)\d+', "`${1}$min"
        $t = $t -creplace '(#define\s+DM_VERSION_PATCH\s+)\d+', "`${1}$pat"
        $t = $t -creplace '(DM_VERSION_FILE_DESCRIPTION\s+L"WM_UMBRA v)\d+\.\d+\.\d+\.\d+(")', "`${1}$maj.$min.$pat.0`${2}"
        $t = $t -creplace '(DM_VERSION_PRODUCT_VALUE\s+L")\d+\.\d+(")',                        "`${1}$maj.$min`${2}"
        $t = $t -creplace '(DM_VERSION_FILE_VALUE\s+L")\d+\.\d+\.\d+\.\d+(")',                 "`${1}$maj.$min.$pat.0`${2}"
        $t
    }
    Edit-File 'CMakeLists.txt' {
        param($t)
        $t -creplace '(project\(umbra\s+VERSION\s+)\d+\.\d+\.\d+', "`${1}$Version"
    }
    Edit-File 'vcpkg-overlay/umbra/vcpkg.json' {
        param($t)
        $t -creplace '("version-semver"\s*:\s*")\d+\.\d+\.\d+(")', "`${1}$Version`${2}"
    }

    Write-Host ""
    Write-Host "Next:" -ForegroundColor Cyan
    Write-Host "  git commit -am `"Bump version to $Version`""
    Write-Host "  git tag -a v$Version -m `"WM_UMBRA v$Version`""
    Write-Host "  git push origin main v$Version"
    Write-Host "  .\bump_version.ps1 $Version -UpdateHash    # after the tag is on GitHub"
}
