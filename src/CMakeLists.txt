add_library(zotero_static STATIC
        zotero.cpp
        zotero_item.h)
set_property(TARGET zotero_static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(zotero_static
        Qt6::Core
        Qt6::Sql
        nlohmann_json::nlohmann_json)

add_library(index_static STATIC index.cpp)
set_property(TARGET index_static PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(index_static
        zotero_static
        Qt6::Core
        Qt6::Sql
        nlohmann_json::nlohmann_json)

#add_library(krunner_zotero_static STATIC krunner_zotero.cpp)
#set_property(TARGET krunner_zotero_static PROPERTY POSITION_INDEPENDENT_CODE ON)
#target_link_libraries(krunner_zotero_static
#        zotero_static
#        index_static
#        KF6::Runner
#        KF6::ConfigCore
#        KF6::KIOWidgets)
#
#kcoreaddons_add_plugin(krunner_zotero SOURCES krunner_zotero.cpp INSTALL_NAMESPACE "kf6/krunner")
#target_link_libraries(krunner_zotero krunner_zotero_static)
#
#install(TARGETS krunner_zotero DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kf${QT_MAJOR_VERSION}/krunner/)
