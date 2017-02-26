#include "MdSpi.h"
#include "TraderSpi.h"
#include <iostream>
#include <sstream>
#include <list>
#include <string.h>
#include <stdlib.h>
#include "globalutil.h"
#include <unordered_map>
#include <boost/chrono.hpp>
using namespace std;
extern CTraderSpi* pUserSpi;
extern unordered_map<string,unordered_map<string,int>> positionmap;
#pragma warning(disable : 4996)
//存放行情消息队列
extern list<string> mkdata;
extern list<string> loglist;
//行情各字段分割符
char sep[] = ";";
// USER_API参数
extern CThostFtdcMdApi* mduserapi;
//连接到服务器端的客户数量
extern int customercount;
// 配置参数
extern char MD_FRONT_ADDR[];		
extern TThostFtdcBrokerIDType	BROKER_ID;
extern TThostFtdcInvestorIDType INVESTOR_ID;
extern TThostFtdcPasswordType	PASSWORD;
extern char** ppInstrumentID;
extern int iInstrumentID;
extern int isclose;
int buytime = 0;
int selltime = 0;
extern int ret;
extern double tick;
extern int bidmultipy;
extern int askmultipy;
extern int pstalarm;
extern int ordervol ;
//买卖价差比较值
extern int bid_ask_spread;
//成交量基数
extern int trade_volume;
// 请求编号
extern int iRequestID;
//上一次成交总量
long totalVolume = 0;
extern int offset_flag;
extern boost::lockfree::queue<LogMsg*> mkdataqueue;
extern boost::lockfree::queue<LogMsg*> logqueue;
extern boost::lockfree::queue<MkDataPrice*> mkdatapricequeue;
void CMdSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo,
		int nRequestID, bool bIsLast)
{
	cerr << "--->>> "<< __FUNCTION__ << endl;
	IsErrorRspInfo(pRspInfo);
}

void CMdSpi::OnFrontDisconnected(int nReason)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> Reason = " << nReason << endl;
}
		
void CMdSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

void CMdSpi::OnFrontConnected()
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	///用户登录请求
	ReqUserLogin();
}

void CMdSpi::ReqUserLogin()
{
	CThostFtdcReqUserLoginField req;
    memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	int iResult = mduserapi->ReqUserLogin(&req, ++iRequestID);
	cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CMdSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << __FUNCTION__ << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		///获取当前交易日
		cerr << "--->>> 获取当前交易日 = " << mduserapi->GetTradingDay() << endl;
		// 请求订阅行情
		SubscribeMarketData();	
	}
}

void CMdSpi::SubscribeMarketData()
{
    int iResult = mduserapi->SubscribeMarketData(ppInstrumentID, iInstrumentID);
	cerr << "--->>> 发送行情订阅请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CMdSpi::OnRspSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "=========================>hello"<<pSpecificInstrument->InstrumentID<<endl;
	string mkinfo;
	mkinfo.append("InstrumentID=");
	mkinfo.append(pSpecificInstrument->InstrumentID);
	cerr << __FUNCTION__ << endl;
}

void CMdSpi::OnRspUnSubMarketData(CThostFtdcSpecificInstrumentField *pSpecificInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << __FUNCTION__ << endl;
}

void CMdSpi::OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData)
{
    OnRtnMarketDataTwo(pDepthMarketData);
    //cout<<"----------------------->"<<getCloseMethod()<<endl;
	//处理行情
	int64_t start_time = GetSysTimeMicros();
    auto chro_start_time = boost::chrono::high_resolution_clock::now();
	string marketdata;
	stringstream ss;

    MkDataPrice* mkdataprice = new MkDataPrice();
    mkdataprice->setAskPrice(pDepthMarketData->AskPrice1);
    mkdataprice->setBidPrice(pDepthMarketData->BidPrice1);
    mkdatapricequeue.push(mkdataprice);

	//处理行情
	int64_t end1 = GetSysTimeMicros();
    string msg;
    /*string msg = "处理信号花费:" + string(GetDiffTime(end1,start_time));
    LogMsg logmsg;
    logmsg.setMsg(msg);
    logqueue.push(&logmsg)*/;
	///交易日
	marketdata.append("TradingDay=");
	marketdata.append(pDepthMarketData->TradingDay);
	marketdata.append(sep);
	///最后修改时间
	marketdata.append("UpdateTime=");
	marketdata.append(pDepthMarketData->UpdateTime);
	marketdata.append(sep);
	///本次成交量
	char char_thisvol[20] = {'\0'};
    int this_trade_vol = pDepthMarketData->Volume;
	sprintf(char_thisvol,"%d",this_trade_vol);
	marketdata.append("this_trade_vol=");
	marketdata.append(char_thisvol);
	marketdata.append(sep);
	///申买价一
	marketdata.append("BidPrice1=");
	ss << pDepthMarketData->BidPrice1;
	string BidPrice1;
	ss >> BidPrice1;
	marketdata.append(BidPrice1);
	marketdata.append(sep);
	ss.clear();
	///申买量一
	char char_bv1[20] = {'\0'};
	sprintf(char_bv1,"%d",pDepthMarketData->BidVolume1);
	marketdata.append("BidVolume1=");
	marketdata.append(char_bv1);
	marketdata.append(sep);
	///申卖价一
	marketdata.append("AskPrice1=");
	ss << pDepthMarketData->AskPrice1;
	string AskPrice1;
	ss >> AskPrice1;
	marketdata.append(AskPrice1);
	marketdata.append(sep);
	ss.clear();
	///申卖量一
	char char_sv1[20] = {'\0'};
	sprintf(char_sv1,"%d",pDepthMarketData->AskVolume1);
	marketdata.append("AskVolume1=");
	marketdata.append(char_sv1);
	marketdata.append(sep);
	marketdata.append("InstrumentID=");
	marketdata.append(pDepthMarketData->InstrumentID);
	marketdata.append(sep);
	marketdata.append("ExchangeID=");
	marketdata.append(pDepthMarketData->ExchangeID);
	marketdata.append(sep);
	///最新价
	marketdata.append("LastPrice=");
	ss << pDepthMarketData->LastPrice;
	string lastprice;
	ss >> lastprice;
	marketdata.append(lastprice);
	marketdata.append(sep);
	///上次结算价
	ss.clear();
	marketdata.append("PreSettlementPrice=");
	ss << pDepthMarketData->PreSettlementPrice;
	string PreSettlementPrice;
	ss >> PreSettlementPrice;
	marketdata.append(PreSettlementPrice);
	marketdata.append(sep);
	///最高价
	ss.clear();
	marketdata.append("HighestPrice=");
	ss << pDepthMarketData->HighestPrice;
	string HighestPrice;
	ss >> HighestPrice;
	marketdata.append(HighestPrice);
	marketdata.append(sep);
	///最低价
	ss.clear();
	marketdata.append("LowestPrice=");
	ss << pDepthMarketData->LowestPrice;
	string LowestPrice;
	ss >> LowestPrice;
	marketdata.append(LowestPrice);
	marketdata.append(sep);
	ss.clear();

	///昨持仓量
	TThostFtdcLargeVolumeType	PreOpenInterest = pDepthMarketData->PreOpenInterest ;
	char char_poi[20] = {'\0'};
	sprintf(char_poi,"%d",PreOpenInterest);
	marketdata.append("PreOpenInterest=");
	marketdata.append(char_poi);
	marketdata.append(sep);
	///今开盘
	marketdata.append("OpenPrice=");
	ss << pDepthMarketData->OpenPrice;
	string OpenPrice;
	ss >> OpenPrice;
	marketdata.append(OpenPrice);
	marketdata.append(sep);
	ss.clear();
	///数量
	TThostFtdcVolumeType	Volume = pDepthMarketData->Volume;
	char char_vol[20] = {'\0'};
	sprintf(char_vol,"%d",Volume);
	marketdata.append("Volume=");
	marketdata.append(char_vol);
	marketdata.append(sep);
	///成交金额
	marketdata.append("Turnover=");
	ss << pDepthMarketData->Turnover;
	string Turnover;
	ss >> Turnover;
	marketdata.append(Turnover);
	marketdata.append(sep);
	ss.clear();
	///持仓量
	TThostFtdcLargeVolumeType	OpenInterest = pDepthMarketData->OpenInterest;
	char char_opi[20] = {'\0'};
	sprintf(char_opi,"%d",OpenInterest);
	marketdata.append("OpenInterest=");
	marketdata.append(char_opi);
	marketdata.append(sep);
	///今收盘
	marketdata.append("ClosePrice=");
	ss << pDepthMarketData->ClosePrice;
	string ClosePrice;
	ss >> ClosePrice;
	marketdata.append(ClosePrice);
	marketdata.append(sep);
	ss.clear();
	///本次结算价
	marketdata.append("SettlementPrice=");
	ss << pDepthMarketData->SettlementPrice;
	string SettlementPrice;
	ss >> SettlementPrice;
	marketdata.append(SettlementPrice);
	marketdata.append(sep);
	ss.clear();
	///涨停板价
	marketdata.append("UpperLimitPrice=");
	ss << pDepthMarketData->UpperLimitPrice;
	string UpperLimitPrice;
	ss >> UpperLimitPrice;
	marketdata.append(UpperLimitPrice);
	marketdata.append(sep);
	ss.clear();
	///跌停板价
	marketdata.append("LowerLimitPrice=");
	ss << pDepthMarketData->LowerLimitPrice;
	string LowerLimitPrice;
	ss >> LowerLimitPrice;
	marketdata.append(LowerLimitPrice);
	marketdata.append(sep);
	ss.clear();
	///昨虚实度
	marketdata.append("PreDelta=");
	ss << pDepthMarketData->PreDelta;
	string PreDelta;
	ss >> PreDelta;
	marketdata.append(PreDelta);
	marketdata.append(sep);
	ss.clear();
	///今虚实度
	marketdata.append("CurrDelta=");
	ss << pDepthMarketData->CurrDelta;
	string CurrDelta;
	ss >> CurrDelta;
	marketdata.append(CurrDelta);
	marketdata.append(sep);
	ss.clear();
	
	///最后修改毫秒
	TThostFtdcMillisecType	UpdateMillisec = pDepthMarketData->UpdateMillisec;
	char char_ums[20] = {'\0'};
	sprintf(char_ums,"%d",UpdateMillisec);
	marketdata.append("UpdateMillisec=");
	marketdata.append(char_ums);
	marketdata.append(sep);
	///当日均价
	marketdata.append("AveragePrice=");
	ss << pDepthMarketData->AveragePrice;
	string AveragePrice;
	ss >> AveragePrice;
	marketdata.append(AveragePrice);
	marketdata.append(sep);
	ss.clear();
//    logmsg.setMsg(marketdata);
//    mkdataqueue.push(&logmsg);

	//处理行情
	int64_t end2 = GetSysTimeMicros();
    auto chro_end_time = boost::chrono::high_resolution_clock::now();
    auto pro_time = boost::chrono::duration<double>(chro_end_time - chro_start_time).count();
    stringstream timss;
    timss<<pro_time;
    cout<<timss.str()<<endl;
    cout<<marketdata<<endl;
    //msg = "记录行情花费:" + string(GetDiffTime(end2,end1)) + ";实际处理时间：" + timss.str();
    LOG(INFO)<<msg;
}

bool CMdSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	// 如果ErrorID != 0, 说明收到了错误的响应
	bool bResult = ((pRspInfo) && (pRspInfo->ErrorID != 0));
	if (bResult)
		cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg << endl;
	return bResult;
}

