qt_add_executable(man_notify EXCLUDE_FROM_ALL main.cpp)
qt_add_qml_module(man_notify
    URI Notify.Playground
    QML_FILES Main.qml
)
target_link_libraries(man_notify PRIVATE
    app_notify app_notifyplugin app_controlsplugin Qt::Quick)
app_target_defaults(man_notify)
