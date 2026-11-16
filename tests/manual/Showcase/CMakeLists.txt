qt_add_executable(showcase EXCLUDE_FROM_ALL showcase.cpp)
target_link_libraries(showcase PRIVATE app_ui app_uiplugin app_controlsplugin app_canvasplugin
    app_sketchbook app_sketchbookplugin app_notify app_notifyplugin
    Qt::Test Qt::Svg)
app_target_defaults(showcase)
