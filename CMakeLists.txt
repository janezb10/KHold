cmake_minimum_required(VERSION 3.10)
project(khold)

find_package(Fcitx5Core 5.1.12 REQUIRED)
find_package(Fcitx5Utils 5.1.12 REQUIRED)

include(GNUInstallDirs)
include(FeatureSummary)

set(KHOLD_SOURCES
    src/khold.cpp
)

add_fcitx5_addon(khold ${KHOLD_SOURCES})
target_link_libraries(khold Fcitx5::Core Fcitx5::Utils)

configure_file(src/khold.conf.in khold.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/khold.conf"
    DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")
install(TARGETS khold DESTINATION "${FCITX_INSTALL_ADDONDIR}")
