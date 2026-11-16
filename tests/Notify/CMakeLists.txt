app_add_qt_test(tst_NotificationManager SOURCES tst_NotificationManager.cpp LIBRARIES app_notify Qt::Gui)
app_add_qt_test(tst_NotificationHost SOURCES tst_NotificationHost.cpp
    LIBRARIES app_notify app_notifyplugin app_controlsplugin Qt::Quick)
