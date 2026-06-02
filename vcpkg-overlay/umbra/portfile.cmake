# WM_UMBRA (umbra) — static library only; the CRT model follows the triplet
# (x64-windows -> dynamic CRT, x64-windows-static -> static CRT).
set(VCPKG_LIBRARY_LINKAGE static)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO martona/WM_UMBRA
    REF "v${VERSION}"
    SHA512 60b355838f1d1fad8f4fb48c85c828448facc92fca039625489c014ea4049e66b02fadf5bdd57a29f082b60581f33ecc9d2fccc8cff8356819dcf25c7f4552bd
    HEAD_REF main
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUMBRA_BUILD_SAMPLE=OFF
        -DUMBRA_INSTALL=ON
)

vcpkg_cmake_install()

vcpkg_cmake_config_fixup(PACKAGE_NAME umbra CONFIG_PATH lib/cmake/umbra)

# Headers are installed from the release pass only.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# MIT overall, with the retained BSD-3-Clause (Anthony Lee Stark) notice.
vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/LICENSE.md"
    "${SOURCE_PATH}/NOTICE.md"
)
