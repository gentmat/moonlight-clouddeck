QT += core webenginecore webenginewidgets
CONFIG += c++11

TEMPLATE = lib
CONFIG += staticlib

TARGET = clouddeck

include(../globaldefs.pri)

SOURCES += \
    src/clouddeckmanager.cpp

HEADERS += \
    src/clouddeckmanager.h

# Make headers available for inclusion
INCLUDEPATH += src

# Export headers for other projects
target.path = /usr/local/lib
headers.files = src/clouddeckmanager.h
headers.path = /usr/local/include/clouddeck

INSTALLS += target headers
