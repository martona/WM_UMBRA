# WM_UMBRA (umbra) — static library only; the CRT model follows the triplet
# (x64-windows -> dynamic CRT, x64-windows-static -> static CRT).
set(VCPKG_LIBRARY_LINKAGE static)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO martona/WM_UMBRA
    REF "v${VERSION}"
    SHA512 98a4acbfbade23832bae5acc88dc8d8c1ad1011b71eecde34c08eee00f1b1e46f156b143cf6e6d61ec4758115a8cb108452120889cb7c30070266413d9d07659
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
