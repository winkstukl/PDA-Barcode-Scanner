QT += core gui widgets serialport

CONFIG += c++11
TEMPLATE = app
TARGET = 0330BARcode

DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    camera.cpp \
    barcode_recognizer.cpp

HEADERS += \
    mainwindow.h \
    camera.h \
    barcode_recognizer.h

# 你交叉编译出来的 zbar 暂存目录
ZBAR_STAGE = /home/alientek/3rdparty/zbar-stage/usr

contains(QT_ARCH, arm) {
    CONFIG += link_pkgconfig
    PKGCONFIG += opencv4
    DEFINES += __arm__

    INCLUDEPATH += $$ZBAR_STAGE/include
    LIBS += -L$$ZBAR_STAGE/lib -lzbar
} else {
    # Ubuntu 本机调试用
    LIBS += -L/usr/local/lib \\
            -lopencv_core \\
            -lopencv_highgui \\
            -lopencv_imgproc \\
            -lopencv_videoio \\
            -lopencv_imgcodecs \\
            -lzbar

    INCLUDEPATH += /usr/local/include/opencv4
    INCLUDEPATH += /usr/include
}

qnx: target.path = /tmp/$${TARGET}/bin
unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
