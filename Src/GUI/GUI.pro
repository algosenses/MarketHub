#-------------------------------------------------
#
# Project created by QtCreator 2014-12-30T15:13:30
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = GUI
TEMPLATE = app


SOURCES += main.cpp\
        dialog.cpp \
        adaptormodel.cpp

HEADERS  += dialog.h adaptormodel.h

FORMS    += dialog.ui

win32:RC_ICONS += favicon.ico
