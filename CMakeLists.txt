cmake_minimum_required(VERSION 3.16)

project(krunner_zotero)
set(CMAKE_PROJECT_VERSION "0.2.1")
add_definitions(-DCMAKE_PROJECT_VERSION="${CMAKE_PROJECT_VERSION}")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/tran-khoa/krunner-zotero")
set(CMAKE_PROJECT_AUTHOR "Viet Anh Khoa Tran")
set(CMAKE_PROJECT_AUTHOR_EMAIL "dev@ktran.de")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS: OFF)
set(QT_MIN_VERSION "6.6.0")
set(KF6_MIN_VERSION "6.5.0")

include(FeatureSummary)

find_package(ECM ${KF6_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP NEW)
FetchContent_MakeAvailable(json)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMGenerateExportHeader)
include(ECMInstallIcons)
include(KDEPackageAppTemplates)
include(GenerateExportHeader)
include(CMakePackageConfigHelpers)
include(KDEClangFormat)
include(KDEGitCommitHooks)

find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
        Core
        Sql
)
find_package(KF6 ${KF6_MIN_VERSION} REQUIRED COMPONENTS
        Config
        KIO
        Runner
)

add_definitions(
        -DQT_DEPRECATED_WARNINGS
        -DQT_NO_URL_CAST_FROM_STRING
)

add_subdirectory(src)
# add_subdirectory(examples)

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Krunner Plugin which allows you to search through your Zotero library.")
set(CPACK_PACKAGE_NAME krunner-${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CMAKE_PROJECT_AUTHOR}")
set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")

set(KRUNNER_VERSION "${KF6Runner_VERSION}")
set(CPACK_PACKAGE_FILE_NAME ${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_OS}-plasma6)
if (CPACK_OS STREQUAL "ubuntu")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libkf6runner6 (>=${KRUNNER_VERSION})")
    set(CPACK_GENERATOR "DEB")
elseif(CPACK_OS STREQUAL "fedora" OR CPACK_OS STREQUAL "opensuse")
    set(CPACK_RPM_PACKAGE_REQUIRES "kf6-krunner >= ${KRUNNER_VERSION}")
    set(CPACK_GENERATOR "RPM")
else()
    unset(CPACK_PACKAGE_FILE_NAME)
endif()
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_CURRENT_SOURCE_DIR}/postinst;${CMAKE_CURRENT_SOURCE_DIR}/postrm")
set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postinst")
set(CPACK_RPM_POST_UNINSTALL_SCRIPT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/postrm")

include(CPack)