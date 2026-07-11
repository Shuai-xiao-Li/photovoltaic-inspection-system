QT += quick quickcontrols2 serialport
CONFIG += c++11
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    src/vehicledata.cpp \
    src/panelmodel.cpp \
    src/detectionbridge.cpp

HEADERS += \
    src/vehicledata.h \
    src/panelmodel.h \
    src/detectionbridge.h

RESOURCES += qml.qrc

TARGET = PhytiumCarUI
TEMPLATE = app
