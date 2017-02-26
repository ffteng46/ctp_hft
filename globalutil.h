#ifndef _TFF_WS2DEF_
#define _TFF_WS2DEF_

#include<iostream>
#include <string.h>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif  // _WIND32
#include <boost/lockfree/queue.hpp>
#include <boost/bind.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread_pool.hpp>
#include <boost/thread.hpp>
#include <chrono>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>
#include <glog/logging.h>
#include <glog/log_severity.h>
#include <boost/locale.hpp>
#include "boost_tools.h"
#include "../ctp/ThostFtdcUserApiStruct.h"
using namespace std;
extern int realLongPstLimit;
extern int realShortPstLimit;
class LogMsg{
public:
    LogMsg(){}
    string& getMsg(){
        return strmsg;
    }
    void setMsg(string msg){
        strmsg = msg;
    }
    int& GetData()
    {
        return m_iData;
    }
    ~LogMsg(){}
private:
    int m_iData;
    string m_szDataString;
    string strmsg;
    //char m_szDataString[MAX_DATA_SIZE];
};
class MkDataPrice{
public:
    MkDataPrice(){}
    ~MkDataPrice(){}
    void setBidPrice(double bidPrice){
        this->bidPrice = bidPrice;
    }
    void setAskPrice(double askPrice){
        this->askPrice = askPrice;
    }
    double getBidPrice(){
        return this->bidPrice;
    }
    double getAskPrice(){
        return this->askPrice;
    }

private:
    double bidPrice;
    double askPrice;
};

void recordRunningMsg(string msg);
void querySleep();
//mkdata trigger action order map
void actionOrderReinsertEngine();
// 定义64位整形
#if defined(_WIN32) && !defined(CYGWIN)
typedef __int64 int64_t;
#else
typedef long long int64t;
#endif  // _WIN32
vector<string> split(string str,string pattern);
void logEngine();				//日志记录器
void marketdataEngine();				//行情记录器

// 获取系统的当前时间，单位微秒(us)
int64_t GetSysTimeMicros();
char* GetDiffTime(int64_t start,int64_t end);//处理时间差

void test();
//获取当前系统时间YYYY-MM-DD HH:MI:SS
string getCurrentSystemTime();
//trim字符两边的空格
void OnRtnMarketDataTwo(CThostFtdcDepthMarketDataField *pDepthMarketData);
string getCloseMethod(string type);
void initPriceGap();
#endif
