qt_add_qml_module(app_notify
    URI App.Notify
    STATIC
    QML_FILES
        NotificationCard.qml
        NotificationDialog.qml
        NotificationHost.qml
    SOURCES
        Notification.cpp Notification.hpp
        NotifyTypes.hpp
        NotificationManager.cpp NotificationManager.hpp
)
target_include_directories(app_notify
    PUBLIC "${PROJECT_SOURCE_DIR}/src"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(app_notify PUBLIC Qt::Core Qt::Qml PRIVATE Qt::Quick app_controls)
app_target_defaults(app_notify)
