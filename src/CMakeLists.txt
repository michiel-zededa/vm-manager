qt_add_executable(vm-manager WIN32 MACOSX_BUNDLE
    main.cpp
    core/Backend.h
    core/Types.h
    core/MockBackend.h        core/MockBackend.cpp
    core/ConnectionManager.h  core/ConnectionManager.cpp
    core/AppController.h      core/AppController.cpp
    core/VmListModel.h        core/VmListModel.cpp
    core/ConnectionModel.h    core/ConnectionModel.cpp
    core/ImageImporter.h      core/ImageImporter.cpp
    core/SnapshotScheduler.h  core/SnapshotScheduler.cpp
)

target_include_directories(vm-manager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(vm-manager PRIVATE PROJECT_VERSION="${PROJECT_VERSION}")

target_link_libraries(vm-manager PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent
)

# Real libvirt backend, compiled only when available.
if(HAVE_LIBVIRT)
    target_sources(vm-manager PRIVATE
        core/LibvirtBackend.h core/LibvirtBackend.cpp)
    target_link_libraries(vm-manager PRIVATE PkgConfig::LIBVIRT)
    target_compile_definitions(vm-manager PRIVATE HAVE_LIBVIRT=1)
endif()

# ---------------------------------------------------------------------------
# QML module
# ---------------------------------------------------------------------------
qt_add_qml_module(vm-manager
    URI VMManager
    VERSION 1.0
    QML_FILES
        ui/qml/Main.qml
        ui/qml/Theme.qml
        ui/qml/components/AppButton.qml
        ui/qml/components/IconButton.qml
        ui/qml/components/Card.qml
        ui/qml/components/StatusBadge.qml
        ui/qml/components/Sparkline.qml
        ui/qml/components/NavItem.qml
        ui/qml/components/SearchField.qml
        ui/qml/components/SectionHeader.qml
        ui/qml/components/EmptyState.qml
        ui/qml/components/StatTile.qml
        ui/qml/components/Toast.qml
        ui/qml/views/DashboardView.qml
        ui/qml/views/VmCard.qml
        ui/qml/views/VmDetailView.qml
        ui/qml/views/CreateWizard.qml
        ui/qml/views/ImportView.qml
        ui/qml/views/ConsoleView.qml
        ui/qml/views/StorageView.qml
        ui/qml/views/NetworkView.qml
        ui/qml/views/SnapshotsView.qml
)

# Theme.qml is a QML singleton.
set_source_files_properties(ui/qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

install(TARGETS vm-manager
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib)
