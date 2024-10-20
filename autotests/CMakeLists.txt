find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Widgets)


set(BUILD_PATH "${CMAKE_SOURCE_DIR}/cmake-build-debug")

link_directories(
        "${BUILD_PATH}"
)
add_executable(test_zotero test_zotero.cpp)
target_include_directories(test_zotero PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_zotero zotero Qt6::Core Qt6::Widgets)

add_executable(test_index test_index.cpp)
target_include_directories(test_index PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(test_index index zotero Qt6::Core Qt6::Widgets)