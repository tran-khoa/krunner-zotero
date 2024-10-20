find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS I18n Config Runner KIO Notifications)
find_package(Qt${QT_MAJOR_VERSION} REQUIRED COMPONENTS Core Sql)

add_library(zotero STATIC zotero.cpp)
target_link_libraries(zotero PUBLIC Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Sql)

add_library(index STATIC index.cpp)
target_link_libraries(index PUBLIC zotero Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Sql)

kcoreaddons_add_plugin(krunner_zotero SOURCES krunner_zotero.cpp zotero.cpp index.cpp INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/krunner")
target_link_libraries(krunner_zotero KF${QT_MAJOR_VERSION}::Runner KF${QT_MAJOR_VERSION}::I18n KF${QT_MAJOR_VERSION}::ConfigCore KF${QT_MAJOR_VERSION}::KIOWidgets KF${QT_MAJOR_VERSION}::Notifications Qt${QT_MAJOR_VERSION}::Core Qt${QT_MAJOR_VERSION}::Sql)

install(TARGETS krunner_zotero DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kf${QT_MAJOR_VERSION}/krunner/)
