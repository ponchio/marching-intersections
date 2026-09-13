#-------------------------------------------------
#
# Project created by QtCreator 2014-08-01T14:23:35
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = marching
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

QMAKE_CXXFLAGS += -std=c++0x
QMAKE_CXXFLAGS += -Wno-unused-local-typedefs

SOURCES += \
    ../../../vcglib/wrap/system/qgetopt.cpp \
    main.cpp \
    intersections.cpp \
    mc_edge.cpp \
    loom.cpp \
    dual_loom.cpp \
    mc_table.cpp

HEADERS += \
    ../../../vcglib/wrap/system/qgetopt.h \
    intersections.h \
    mc_edge.h \
    loom.h \
    dual_loom.h \
    mc_table.h \
    plane.h

INCLUDEPATH += -I ../../../vcglib

DEFINES += STORE_NORMALS=1
