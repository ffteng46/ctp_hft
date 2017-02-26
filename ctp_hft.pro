TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
    MdSpi.cpp \
    OrderInsert.cpp \
    Trade.cpp \
    TraderSpi.cpp \
    globalutil.cpp \
    testTraderApi.cpp \
    boost_tools.cpp
INCLUDEPATH += ../../boost_1_61_0
LIBS += -L../../boost_1_61_0/stage/lib
LIBS += -L../../ctp/ -lthosttraderapi -lthostmduserapi
LIBS += -lboost_system -lboost_thread -lthosttraderapi -lthostmduserapi -lpthread -lboost_chrono -lglog -lboost_locale
HEADERS += \
    ../../ctp/ThostFtdcMdApi.h \
    ../../ctp/ThostFtdcTraderApi.h \
    ../../ctp/ThostFtdcUserApiDataType.h \
    ../../ctp/ThostFtdcUserApiStruct.h \
    MdSpi.h\
    globalutil.h \
    OrderInsert.h \
    Trade.h \
    TraderSpi.h \
    ../../ctp/log_severity.h \
    ../../ctp/logging.h \
    boost_tools.h


