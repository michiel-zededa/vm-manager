# CPack configuration. Produces a platform-appropriate installer via
# `cmake --build build --target package`.
#
# The GitHub release workflow runs the Qt deploy tools first so the packaged
# artifact is self-contained (bundled Qt runtime).

set(CPACK_PACKAGE_NAME "VM Manager")
set(CPACK_PACKAGE_VENDOR "VM Manager contributors")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "${PROJECT_HOMEPAGE_URL}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "VM Manager")
set(CPACK_PACKAGE_CONTACT "michiel@zededa.com")

if(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "VM Manager")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_MODIFY_PATH ON)
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_DISPLAY_NAME "VM Manager")
else()
    # Linux: TGZ + DEB out of the box; AppImage is produced by the release
    # workflow via linuxdeploy.
    set(CPACK_GENERATOR "TGZ;DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "michiel@zededa.com")
    set(CPACK_DEBIAN_PACKAGE_SECTION "admin")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
endif()

include(CPack)
