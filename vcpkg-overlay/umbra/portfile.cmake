# WM_UMBRA (umbra) — static library only; the CRT model follows the triplet
# (x64-windows -> dynamic CRT, x64-windows-static -> static CRT).
set(VCPKG_LIBRARY_LINKAGE static)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO martona/WM_UMBRA
    REF "v${VERSION}"
    SHA512 74e0f15f1a861e1ed29bd6722b23d38dd9690be70239224cd317bc71a16a0fe5c24f5cf9c5eba4011e58c2ab0428be9b0f693c8be8840a42f593b82da11ced4a
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
