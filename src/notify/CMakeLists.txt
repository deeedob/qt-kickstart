set_source_files_properties(SeverityBadge.qml PROPERTIES QT_QML_INTERNAL_TYPE TRUE)

qt_add_qml_module(app_notify
    URI App.Notify
    STATIC
    QML_FILES
        Card.qml
        Dialog.qml
        SeverityBadge.qml
        Host.qml
    SOURCES
        Notification.cpp Notification.hpp
        Namespace.hpp
        Manager.cpp Manager.hpp
)
target_include_directories(app_notify
    PUBLIC "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(app_notify PUBLIC Qt::Core Qt::Qml PRIVATE Qt::Quick app_controls)
app_target_defaults(app_notify)
