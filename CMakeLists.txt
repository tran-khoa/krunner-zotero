cmake_minimum_required(VERSION 3.29)
project(krunner_zotero)

set(KF_MIN_VERSION "5.91.0")
set(QT_MAJOR_VERSION "6")

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED COMPONENTS I18n Config Runner KIO Notifications)


kcoreaddons_add_plugin(krunner_zotero SOURCES krunner_zotero.cpp INSTALL_NAMESPACE "kf${QT_MAJOR_VERSION}/krunner")
target_link_libraries(krunner_zotero KF${QT_MAJOR_VERSION}::Runner KF${QT_MAJOR_VERSION}::I18n KF${QT_MAJOR_VERSION}::ConfigCore KF${QT_MAJOR_VERSION}::KIOWidgets KF${QT_MAJOR_VERSION}::Notifications)

MESSAGE(${KDE_INSTALL_QTPLUGINDIR}/kf${QT_MAJOR_VERSION}/krunner/)
install(TARGETS krunner_zotero DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kf${QT_MAJOR_VERSION}/krunner/)