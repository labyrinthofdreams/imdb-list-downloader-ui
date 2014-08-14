#-------------------------------------------------
#
# Project created by QtCreator 2014-08-12T18:58:41
#
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = IMDbDownloader
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp

HEADERS  += mainwindow.hpp

FORMS    += mainwindow.ui

QMAKE_CXXFLAGS += -std=c++11 -Wall -Wextra -Wpedantic
