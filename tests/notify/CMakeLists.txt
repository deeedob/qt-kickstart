app_add_qt_test(tst_Manager SOURCES tst_Manager.cpp LIBRARIES app_notify Qt::Gui)
app_add_qt_test(tst_Host SOURCES tst_Host.cpp
    LIBRARIES app_notify app_notifyplugin app_controlsplugin Qt::Quick)
