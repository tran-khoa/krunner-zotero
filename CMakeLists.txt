cmake_minimum_required(VERSION 3.16)

project(krunner_zotero)
set(CMAKE_PROJECT_VERSION "0.1.0")
add_definitions(-DCMAKE_PROJECT_VERSION="${CMAKE_PROJECT_VERSION}")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/tran-khoa/krunner-zotero")
set(CMAKE_PROJECT_AUTHOR "Viet Anh Khoa Tran")
set(CMAKE_PROJECT_AUTHOR_EMAIL "dev@ktran.de")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS: OFF)
set(QT_MIN_VERSION "6.7.0")
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
if (BUILD_TESTING)
    add_subdirectory(autotests)
endif ()

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.cpp *.h)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)