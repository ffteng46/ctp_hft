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
string getCloseMethod();
extern boost::lockfree::queue<LogMsg*> mkdataqueue;
extern boost::lockfree::queue<LogMsg*> logqueue;
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
	cout<<"----------------------->"<<getCloseMethod()<<endl;
	//处理行情
	int64_t start_time = GetSysTimeMicros();
    auto chro_start_time = boost::chrono::high_resolution_clock::now();
	string marketdata;
	stringstream ss;
	//买量-卖量差值
	int sprd = pDepthMarketData->BidVolume1 - pDepthMarketData->AskVolume1;
	int abst = std::abs(sprd);
	char *char_inst= pDepthMarketData->InstrumentID;
	string str_inst = string(char_inst);
	bool is_staight_insert = false;//没有持仓直接报单
    unordered_map<string,unordered_map<string,int>>::iterator map_strs = positionmap.find(str_inst);//持仓信息
    for(unordered_map<string,unordered_map<string,int>>::iterator it=positionmap.begin();it != positionmap.end();it ++){
		string tmpmsg;
		tmpmsg.append(it->first).append("持仓情况:");
		char char_tmp_pst[10] = {'\0'};
		sprintf(char_tmp_pst,"%d",it->second["longTotalPosition"]);
		tmpmsg.append("多头数量=");
		tmpmsg.append(char_tmp_pst);
		char char_tmp_pst2[10] = {'\0'};
		sprintf(char_tmp_pst2,"%d",it->second["shortTotalPosition"]);
		tmpmsg.append("空头数量=");
		tmpmsg.append(char_tmp_pst2);
		cout<<tmpmsg<<endl;
	}
	//多头持仓
	int longTotalPosition = 0;
	//空头持仓
	int shortTotalPosition = 0;
	if(map_strs == positionmap.end()){//没有查询到,直接下单
		cout<<"can't find instrumentid:"<<str_inst<<",there is no position,staight to order insert!"<<endl;
		is_staight_insert = true;
		marketdata.append("strait;");
	}else{
		//多头持仓
		longTotalPosition = map_strs->second["longTotalPosition"];
		//空头持仓
		shortTotalPosition = map_strs->second["shortTotalPosition"];
	}
	double mk_bidprice = pDepthMarketData->BidPrice1;
	double mk_askprice = pDepthMarketData->AskPrice1;
	long this_trade_vol = 0;//本次成交量
	if(totalVolume == 0){
		totalVolume = pDepthMarketData->Volume;
	}else{
		this_trade_vol = pDepthMarketData->Volume - totalVolume;
		totalVolume = pDepthMarketData->Volume;
	}
	if(abst >= bid_ask_spread && sprd > 0 && this_trade_vol >= trade_volume){//买强
		//判断用行情价格
		double bidprice = mk_bidprice + tick*bidmultipy;
		if(is_staight_insert){//直接下单
			if(bidprice <= mk_askprice){
				if(isclose == 1){//开仓
					char dir[]="0";
					char offset[] = "0";
					pUserSpi->md_orderinsert(bidprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << bidprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
					
				}else if(isclose == 2){//平仓
					char dir[]="0";
					char offset[] = "1";
					pUserSpi->md_orderinsert(bidprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << bidprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}
			}else{
				if(isclose == 1){//开仓
					char dir[]="0";
					char offset[] = "0";
					pUserSpi->md_orderinsert(mk_askprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << mk_askprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}else if(isclose == 2){//平仓
					char dir[]="0";
					char offset[] = "1";
					pUserSpi->md_orderinsert(mk_askprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << mk_askprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}
			}
		}else{//不直接下单，要判断
			
			int order_vol = 0;
			int subpositon = longTotalPosition - shortTotalPosition;
			int yuzhi = std::abs(subpositon)/pstalarm;
			char char_yuzhi[10] = {'\0'};
			sprintf(char_yuzhi,"%d",yuzhi);
			char char_sub[10]={'\0'};
			sprintf(char_sub,"%d",subpositon);
			//多头持仓
			char char_long[10]={'\0'};
			sprintf(char_long,"%d",longTotalPosition);
			//空头持仓
			char char_short[10]={'\0'};
			sprintf(char_short,"%d",shortTotalPosition);
			//买
			char char_orderdir[] = "0";
			//开平
			char char_orderoffset[3]={'\0'};
			string orderoffset;
			//价格
			double orderprice = 0;
			if(subpositon >= 0){
				if(yuzhi == 0){
					order_vol = ordervol;
					//价格判断
					if(bidprice <= mk_askprice){
						orderprice = bidprice;
					}else{
						orderprice = mk_askprice;//等于卖一价
					}
					//开平判断
					if(isclose == 1){//开仓  
						orderoffset = "0";
					}else if(isclose == 2){//平仓  开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else{
					order_vol = ordervol;
					//价格判断
					if(bidprice < mk_askprice){
						orderprice = bidprice;
						//开平判断
						if(isclose == 1){//开仓
						    orderoffset = "0";
						}else if(isclose == 2){//平仓  开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
							orderoffset = getCloseMethod();
						}
						strcpy(char_orderoffset,orderoffset.c_str());
						pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
					}
				}
			}else if(subpositon <= 0){//卖仓暴露风险
				//double orderprice = 0;
				//char char_orderdir[] = "0";
				//char char_orderoffset[3] = {'\0'};
				//string orderoffset;
				if(yuzhi == 0){
					order_vol = ordervol;
					//价格判断
					if(bidprice <= mk_askprice){
						orderprice = bidprice;
					}else{
						orderprice = mk_askprice;//等于卖一价
					}
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓  开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi == 1){
					order_vol = ordervol*pstalarm - 1;
					//价格判断
					if(bidprice <= mk_askprice){
						orderprice = bidprice;
					}else{
						orderprice = mk_askprice;//等于卖一价
					}
					//开平判断
					if(isclose == 1){//开仓
					    orderoffset = "0";
					}else if(isclose == 2){//平仓  开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi == 2){
					order_vol = ordervol*pstalarm*2 - 1;
					//价格判断
					if(bidprice <= mk_askprice){
					    orderprice = bidprice;
					}else{
					    orderprice = mk_askprice;//等于卖一价
					}
					//开平判断
					if(isclose == 1){//开仓
					    orderoffset = "0";
					}else if(isclose == 2){//平仓
					    orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi > 2){
					order_vol = ordervol*pstalarm*2 + 1;
					//价格判断
					orderprice = mk_askprice;//卖一价尽快成交
					//开平判断
					if(isclose == 1){//开仓
					    orderoffset = "0";
					}else if(isclose == 2){//平仓
					    orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}
			}
			marketdata.append(";卖仓=").append(char_short  );
			marketdata.append(";买仓=").append(char_long );
			marketdata.append(";yuzhi=").append(char_yuzhi);
			marketdata.append(";买强持仓差=").append(char_sub);
			marketdata.append(";direction=").append(char_orderdir);
			marketdata.append(";offset=").append(char_orderoffset);
			marketdata.append(";price=");
			ss << orderprice;
			string tmpstr;
			ss >> tmpstr;
			marketdata.append(tmpstr);
			marketdata.append(sep);
			ss.clear();
			char char_poi[6] = {'\0'};
			sprintf(char_poi,"%d",order_vol);
			marketdata.append("ordervol=");
			marketdata.append(char_poi);
			marketdata.append(sep);
		}	
	}
	if(abst >= bid_ask_spread && sprd < 0 && this_trade_vol >= trade_volume){//卖强
		//判断用行情价格
		double askprice = mk_askprice - tick* askmultipy;
		if(is_staight_insert){//直接下单
			if(askprice >= mk_bidprice){
				if(isclose == 1){//开仓
					char dir[]="1";
					char offset[] = "0";
					pUserSpi->md_orderinsert(askprice,dir,offset,char_inst,ordervol);

					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << askprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}else if(isclose == 2){//平仓
					char dir[]="1";
					char offset[] = "1";
					pUserSpi->md_orderinsert(askprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << askprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}
			}else{
				if(isclose == 1){//开仓
					char dir[]="1";
					char offset[] = "0";
					pUserSpi->md_orderinsert(mk_bidprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << mk_bidprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}else if(isclose == 2){//平仓
					char dir[]="1";
					char offset[] = "1";
					pUserSpi->md_orderinsert(mk_bidprice,dir,offset,char_inst,ordervol);
					marketdata.append("direction=").append(dir);
					marketdata.append("offset=").append(offset);
					marketdata.append("price=");
					ss << mk_bidprice;
					string tmpstr;
					ss >> tmpstr;
					marketdata.append(tmpstr);
					marketdata.append(sep);
					ss.clear();
					char char_poi[6] = {'\0'};
					sprintf(char_poi,"%d",ordervol);
					marketdata.append("ordervol=");
					marketdata.append(char_poi);
					marketdata.append(sep);
				}
			}
		}else{//不直接下单，要判断
			int order_vol = 0;//实际下单量
			double orderprice = 0;
			char char_orderoffset[3] = {'\0'};
			string orderoffset;
			int subpositon = shortTotalPosition - longTotalPosition;
			int yuzhi = std::abs(subpositon)/pstalarm;
			char char_yuzhi[10] = {'\0'};
			sprintf(char_yuzhi,"%d",yuzhi);
			char char_sub[10]={'\0'};
			sprintf(char_sub,"%d",subpositon);
			char char_long[10]={'\0'};
			sprintf(char_long,"%d",longTotalPosition);
			char char_short[10]={'\0'};
			sprintf(char_short,"%d",shortTotalPosition);
			char char_orderdir[] = "1";
			if(subpositon >= 0){//正向仓位多
				if(yuzhi == 0){
					order_vol = ordervol;
					//价格判断
					if(askprice >= mk_bidprice){
						orderprice = askprice;
					}else{
						orderprice = mk_bidprice;//等于买一价
					}
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else{
					order_vol = ordervol;
					//价格判断
					if(askprice > mk_bidprice){
						orderprice = askprice;
						//开平判断
						if(isclose == 1){//开仓
							orderoffset = "0";
						}else if(isclose == 2){//平仓
							orderoffset = getCloseMethod();
						}
						strcpy(char_orderoffset,orderoffset.c_str());
						pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
					}
				}
			}else if(subpositon <= 0){//买仓暴露风险，卖仓增加下单量
				if(yuzhi == 0){
					order_vol = ordervol;
					//价格判断
					if(askprice >= mk_bidprice){
						orderprice = askprice;
					}else{
						orderprice = mk_bidprice;//等于买一价
					}
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi == 1){
					order_vol = ordervol*pstalarm - 1;
					//价格判断
					if(askprice >= mk_bidprice){
						orderprice = askprice;
					}else{
						orderprice = mk_bidprice;//等于买一价
					}
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi == 2){
					order_vol = ordervol*pstalarm*2 - 1;
					//价格判断
					if(askprice >= mk_bidprice){
						orderprice = askprice;
					}else{
						orderprice = mk_bidprice;//等于买一价
					}
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}else if(yuzhi > 2){
					order_vol = ordervol*pstalarm*2 + 1;
					//价格判断
					orderprice = mk_bidprice;//买一价尽快成交
					//开平判断
					if(isclose == 1){//开仓
						orderoffset = "0";
					}else if(isclose == 2){//平仓
						orderoffset = getCloseMethod();
					}
					strcpy(char_orderoffset,orderoffset.c_str());
					pUserSpi->md_orderinsert(orderprice,char_orderdir,char_orderoffset,char_inst,order_vol);
				}
			}
			marketdata.append(";卖仓=").append(char_short  );
			marketdata.append(";买仓=").append(char_long );
			marketdata.append(";yuzhi=").append(char_yuzhi);
			marketdata.append(";卖强持仓差=").append(char_sub);
			marketdata.append(";direction=").append(char_orderdir);
			marketdata.append(";offset=").append(char_orderoffset);
			marketdata.append(";price=");
			ss << orderprice;
			string tmpstr;
			ss >> tmpstr;
			marketdata.append(tmpstr);
			marketdata.append(sep);
			ss.clear();
			char char_poi[6] = {'\0'};
			sprintf(char_poi,"%d",order_vol);
			marketdata.append("ordervol=");
			marketdata.append(char_poi);
			marketdata.append(sep);
		}
	}
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
	
	/*
	///申买价二
	TThostFtdcPriceType	BidPrice2;
	///申买量二
	TThostFtdcVolumeType	BidVolume2;
	///申卖价二
	TThostFtdcPriceType	AskPrice2;
	///申卖量二
	TThostFtdcVolumeType	AskVolume2;
	///申买价三
	TThostFtdcPriceType	BidPrice3;
	///申买量三
	TThostFtdcVolumeType	BidVolume3;
	///申卖价三
	TThostFtdcPriceType	AskPrice3;
	///申卖量三
	TThostFtdcVolumeType	AskVolume3;
	///申买价四
	TThostFtdcPriceType	BidPrice4;
	///申买量四
	TThostFtdcVolumeType	BidVolume4;
	///申卖价四
	TThostFtdcPriceType	AskPrice4;
	///申卖量四
	TThostFtdcVolumeType	AskVolume4;
	///申买价五
	TThostFtdcPriceType	BidPrice5;
	///申买量五
	TThostFtdcVolumeType	BidVolume5;
	///申卖价五
	TThostFtdcPriceType	AskPrice5;
	///申卖量五
	TThostFtdcVolumeType	AskVolume5;
	*/
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
string getCloseMethod(){
	//平仓  开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
	string orderoffset = "1";
	if(offset_flag == 1){
		orderoffset = "1";
	}else if(offset_flag == 3){
		orderoffset = "3";
	}else if(offset_flag == 4){
		orderoffset = "4";
	}
	return orderoffset;
}
