// testTraderApi.cpp : 定义控制台应用程序的入口点。
//
#include "../../ctp/ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include <list>
#include <string.h>
#include <iostream>
#include "globalutil.h"
#include "Trade.h"
using namespace std;


void initThread(int sendtype);//发送类型 0：以客户端方式发送；1，以服务端方式发送

///日志消息队列
//extern list<string> loglist;
void initThread(int sendtype);
int main(void)
{
   // std::locale::global(std::locale(""));
//	initThread(1);
    google::InitGoogleLogging("");
   // google::SetLogDestination(google::glog_internal_namespace_)
    google::SetLogDestination(0,"./info");
    LOG(INFO)<<"test";
	TradeProcess tp;
	tp.startTrade();
    google::ShutdownGoogleLogging();
    return 0;
}
//void initThread(int sendtype)
//{
//	HANDLE loghdl = CreateThread(NULL,0,logEngine,NULL,0,NULL);
//	CloseHandle(loghdl);
//	HANDLE loghdl2 = CreateThread(NULL,0,marketdataEngine,NULL,0,NULL);
//	CloseHandle(loghdl2);
//}

