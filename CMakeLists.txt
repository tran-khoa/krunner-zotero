cmake_minimum_required(VERSION 3.29)
project(krunner_zotero)

set(KF_MIN_VERSION "5.91.0")
set(QT_MAJOR_VERSION "6")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
if (BUILD_TESTING)
    add_subdirectory(autotests)
endif ()