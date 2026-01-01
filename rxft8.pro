QT += core network
QT -= gui

TARGET = rxft8
CONFIG += console
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += main.cpp \
    rxft8.cpp \
    psk_reporter.cpp

HEADERS += \
    rxft8.h \
    psk_reporter.h

