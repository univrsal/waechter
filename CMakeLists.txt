cmake_minimum_required(VERSION 3.16)

project(waechter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WAECHTER_WITH_WEBSOCKETSERVER "Enable websocket server" ON)
option(WAECHTER_WITH_WEBSOCKETCLIENT "Enable websocket client" ON)

add_subdirectory(Thirdparty)
add_subdirectory(Source)

string(TIMESTAMP TODAY "%Y.%m.%d %H:%M")

execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE)

target_compile_definitions(waechterd PRIVATE
        GIT_BRANCH="${GIT_BRANCH}   "
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        WAECHTER_VERSION="${CMAKE_PROJECT_VERSION}"
        BUILD_TIME="${TODAY}"
)

target_compile_definitions(waechter PUBLIC
        GIT_BRANCH="${GIT_BRANCH}   "
        GIT_COMMIT_HASH="${GIT_COMMIT_HASH}"
        WAECHTER_VERSION="${CMAKE_PROJECT_VERSION}"
        BUILD_TIME="${TODAY}"
        ICON_PATH="${CMAKE_SOURCE_DIR}/Meta/Icon.png"
        FONT_PATH="${CMAKE_SOURCE_DIR}/Thirdparty/jetbrains-mono/JetBrainsMono-Regular.ttf"
)

# Install additional files
install(FILES Meta/Package/waechter.desktop
        DESTINATION share/applications)

install(FILES Meta/Icon.png
        DESTINATION share/icons/hicolor/256x256/apps
        RENAME waechter.png)

install(FILES Meta/Package/waechterd.service
        DESTINATION lib/systemd/system)

install(FILES Meta/Package/waechterd.ini
        DESTINATION /etc/waechter)

# CPack configuration for .deb packages
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "waechter")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "waechter@example.com")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Traffic shaping and monitoring for GNU/Linux")
set(CPACK_PACKAGE_DESCRIPTION "Waechter is a traffic shaping and monitoring tool for GNU/Linux that uses eBPF for efficient packet processing.")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Waechter Developers")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "systemd, libbpf0 | libbpf1, libxkbcommon0, libgl1, libx11-6, libxrandr2, libxinerama1, libxcursor1, libxi6, libcurl4 | libcurl3-gnutls, zlib1g")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/Meta/Package/postinst;${CMAKE_SOURCE_DIR}/Meta/Package/prerm;${CMAKE_SOURCE_DIR}/Meta/Package/postrm")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)

