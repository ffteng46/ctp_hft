#include <iostream>


#include "../../ctp/ThostFtdcTraderApi.h"
#include "TraderSpi.h"
#include <fstream>
#include <unordered_map>
#include <string.h>
#include "../../ctp/ThostFtdcMdApi.h"
#include "globalutil.h"
#include "MdSpi.h"
#include <boost/thread/recursive_mutex.hpp>
#include <boost/algorithm/string.hpp>
extern unordered_map<string,unordered_map<string,int>> positionmap;
#pragma warning(disable : 4996)
using namespace std;
// USER_API参数
extern CThostFtdcTraderApi* pUserApi;
// UserApi行情对象
CThostFtdcMdApi* mduserapi;
// 配置参数;
extern char MD_FRONT_ADDR[];
extern char FRONT_ADDR[];		// 前置地址
extern char BROKER_ID[];		// 经纪公司代码;
extern char INVESTOR_ID[];		// 投资者代码;
extern char PASSWORD[];			// 用户密码;
extern char INSTRUMENT_ID[];	// 合约代码;
extern TThostFtdcPriceType	LIMIT_PRICE;	// 价格;
extern TThostFtdcDirectionType	DIRECTION;	// 买卖方向;
extern int orderref;
extern double tick;
int isclose=0;
int offset_flag = 0;
extern int limit_volume;
// 请求编号;
extern int iRequestID;
///日志消息队列
//extern list<string> loglist;
//跌停价格
extern double min_price;
//涨停价格
extern double max_price;
//未成功录入报单
vector<CThostFtdcInputOrderField*> vecFailedHedgeOrderInsert;
//对冲报单录入列表
vector<CThostFtdcInputOrderField> vecHedgeOrderInsert;
//对冲报单requestid记录
unordered_map<int,int> mapRequestid;
//保存所有对冲记录到hashmap当中
unordered_map<int,CThostFtdcInputOrderField*> mapHedgeOrderInsert;
//持仓是否已经写入定义字段
bool isPositionDefFieldReady = false;
//成交文件是否已经写入定义字段
bool isTradeDefFieldReady = false;
//用户对冲报单文件是否已经写入定义字段
bool isOrderInsertDefFieldReady = false;
//将成交信息组装成对冲报单
CThostFtdcInputOrderField* assamble(CThostFtdcOrderField *pTrade);
////将投资者对冲报单信息写入文件保存
void saveInvestorOrderInsertHedge(CThostFtdcInputOrderField *order,string filepath);
//保存报单回报信息
void saveRspOrderInsertInfo(CThostFtdcInputOrderField *pInputOrder);
//提取投资者报单信息
string getInvestorOrderInsertInfo(CThostFtdcInputOrderField *order);
//将交易所报单回报响应写入文件保存
void saveRtnOrder(CThostFtdcOrderField *pOrder);
//获取交易所响应信息
string getRtnOrder(CThostFtdcOrderField *pOrder);
//日志保存路径
string logpath = "b:\\test\\";
// 会话参数;
TThostFtdcFrontIDType	FRONT_ID;	//前置编号;
//int	FRONT_ID;	//前置编号;
TThostFtdcSessionIDType	SESSION_ID;	//会话编号;
TThostFtdcOrderRefType	ORDER_REF;	//报单引用;
void writeStr(string str);
void write(string str,string filepath);
int storeInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition);
//保存投资者成交信息
string storeInvestorTrade(CThostFtdcTradeField *pTrade);
//c处理成交 
int processtrade(CThostFtdcTradeField *pTrade);
void tradeParaProcessTwo();
//初始化持仓信息
void initpst(CThostFtdcInvestorPositionField *pInvestorPosition);
int ret = 0;
int start_process = 0;
extern unordered_map<string,unordered_map<string,int64_t>> seq_map_orderref;
extern unordered_map<string,string> seq_map_ordersysid;
extern char char_front_id[12];
extern char char_session_id[20];
extern string str_front_id;
extern string str_sessioin_id;
extern bool isrtntradeprocess;
extern boost::lockfree::queue<LogMsg*> logqueue;
extern char singleInstrument[30];

//买平标志,1开仓；2平仓
extern int longPstIsClose;
extern int shortPstIsClose;
//positionmap可重入锁
boost::recursive_mutex pst_mtx;
//orderinsertkey与ordersysid对应关系锁
boost::recursive_mutex order_mtx;
//action order lock
boost::recursive_mutex actionOrderMTX;
//longpstlimit
extern int longpstlimit;
//shortpstlimit
extern int shortpstlimit;
//记录时间 
extern int long_offset_flag;
extern int short_offset_flag;
//卖出报单触发信号
extern int askCulTimes;
//买入报单触发信号
extern int bidCulTimes;
//上涨
extern int up_culculate;
//下跌
extern int down_culculate;
//报单触发信号
extern int cul_times;
int realLongPstLimit = 0;
int realShortPstLimit = 0;
int lastABSSpread = 0;
int firstGap = 2;
int secondGap = 5;
boost::atomic_int32_t orderID(0);
//delayed ask order map
unordered_map<double,unordered_map<string,int32_t>> delayedAskOrderMap;
unordered_map<double,unordered_map<string,int32_t>> delayedBidOrderMap;
//all delayed order
unordered_map<int,CThostFtdcInputOrderField*> allDelayedOrder;
int CTraderSpi::md_orderinsert(CThostFtdcInputOrderField* req){
    int nRequestID = ++iRequestID;
    int iResult = pUserApi->ReqOrderInsert(req,nRequestID);
    cerr << "--->>> ReqOrderInsert:" << ((iResult == 0) ? "成功" : "失败") << endl;
}

int CTraderSpi::md_orderinsert(double price,char *dir,char *offset,char * ins,int ordervolume){
	char InstrumentID[31];
	strcpy(InstrumentID,ins);
	char Direction[2];
	strcpy(Direction,dir);
	///投机 '1';套保'3'
	char HedgeFlag[]="1";
	//组合开平标志: 开仓 '0';平仓 '1';平今 '3';平昨 '4';强平 '2'
	char OffsetFlag[2];
	strcpy(OffsetFlag,offset);
	///价格 double
	TThostFtdcPriceType Price = price;
	//开仓手数
	int Volume = ordervolume;
	//报单引用编号
	sprintf(ORDER_REF,"%d",orderref);
	cout<<"------->"<<ORDER_REF<<endl;
	orderref++;
	//报单结构体
	CThostFtdcInputOrderField req;
    memset(&req,0,sizeof(req));
	///经纪公司代码
	strcpy(req.BrokerID, BROKER_ID);
	///投资者代码
	strcpy(req.InvestorID, INVESTOR_ID);
	///合约代码
	strcpy(req.InstrumentID, InstrumentID);
	///报单引用
	strcpy(req.OrderRef, ORDER_REF);
	
	///用户代码
	//	TThostFtdcUserIDType	UserID;
	strcpy(req.UserID,INVESTOR_ID);
	///报单价格条件: 限价
	req.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    ///买卖方向:
    ///
    req.Direction = Direction[0];
    //strcpy(req.Direction,dir);
	///组合开平标志: 开仓
	strcpy(req.CombOffsetFlag ,offset);
	///组合投机套保标志
	strcpy(req.CombHedgeFlag,HedgeFlag);
	///价格
	req.LimitPrice = Price;
	///数量: 1
	req.VolumeTotalOriginal = Volume;
	///有效期类型: 当日有效
	//req.TimeCondition = THOST_FTDC_TC_GFD;
	req.TimeCondition = THOST_FTDC_TC_IOC;
	///GTD日期
	//TThostFtdcDateType	GTDDate;
	strcpy(req.GTDDate,"");
	///成交量类型: 任何数量
	req.VolumeCondition = THOST_FTDC_VC_AV;
	///最小成交量: 1
	req.MinVolume = 1;
	///触发条件: 立即
	req.ContingentCondition = THOST_FTDC_CC_Immediately;
	///止损价
	//TThostFtdcPriceType	StopPrice;
	req.StopPrice = 0;
	///强平原因: 非强平
	req.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///自动挂起标志: 否
	req.IsAutoSuspend = 0;
	///业务单元
	//	TThostFtdcBusinessUnitType	BusinessUnit;
	strcpy(req.BusinessUnit,"");
	///请求编号
	//	TThostFtdcRequestIDType	RequestID;
	///用户强评标志: 否
	req.UserForceClose = 0;///经纪公司代码

	int nRequestID = ++iRequestID;
	char char_order_index[10]={'\0'};
	sprintf(char_order_index,"%d",nRequestID);
	req.RequestID = nRequestID;

	nRequestID = ++iRequestID;
	char char_query_index[10]={'\0'};
	sprintf(char_query_index,"%d",nRequestID);

	//下单开始时间
    int64_t ist_time = GetSysTimeMicros();
    //auto ist_time = boost::chrono::high_resolution_clock::now();
	//orderinsertkey
	string str_osk = str_front_id + str_sessioin_id + string(req.OrderRef);
    unordered_map<string,int64_t> tmpmap ;
	tmpmap["ist_time"] = ist_time;
	seq_map_orderref[str_osk] = tmpmap;
	//委托类操作，使用客户端定义的请求编号格式
	int iResult = pUserApi->ReqOrderInsert(&req,nRequestID);
	cerr << "--->>> ReqOrderInsert:" << ((iResult == 0) ? "成功" : "失败") << endl;
	string msg = "order_requestid=" + string(char_order_index) + ";orderinsert_requestid=" + string(char_query_index);
//    LogMsg logmsg;
//    logmsg.setMsg(msg);
//    logqueue.push(&logmsg);
    LOG(INFO)<<msg;
	return 0;
}
void CTraderSpi::OnFrontConnected()
{
	cerr << "--->>> " << "OnFrontConnected" << endl;
	///用户登录请求;
    ReqUserLogin();
}
//前置了断开
void CTraderSpi:: OnFrontDisconnected(int nReason)
{
    cerr << "OnFrontDisconnected"<<FRONT_ADDR ;
    char char_msg[1028] = {'\0'};
    if(nReason == 4097){
        cerr << "--->>>前置连接失败： Reason = " << nReason <<",网络读失败";
        sprintf(char_msg,"--->>>前置连接失败： Reason = %d,网络读失败",nReason);
    }else if(nReason == 4098){
        cerr << "--->>>前置连接失败： Reason = " << nReason <<",网络写失败";
    }if(nReason == 8193){
        cerr << "--->>>前置连接失败： Reason = " << nReason <<",接受心跳超时";
    }if(nReason == 8194){
        cerr << "--->>>前置连接失败： Reason = " << nReason <<",发送心跳失败";
    }if(nReason == 8195){
        cerr << "--->>>前置连接失败： Reason = " << nReason <<",收到错误报文";
    }
//    LogMsg d;
    string msg2 = boosttoolsnamespace::CBoostTools::gbktoutf8(string(char_msg));
    /*d.setMsg(msg2);
//    logqueue.push( &d */
    LOG(INFO)<<msg2;
    sleep(200);
}

void CTraderSpi::ReqUserLogin()
{
    cout<<PASSWORD<<":"<<BROKER_ID<<" "<<INVESTOR_ID<<" "<<PASSWORD<<endl;
    char char_msg[1024];
    sprintf(char_msg, "发送用户登录请求:BROKER_ID:%s;INVESTOR_ID:%s;PASSWORD:%s",BROKER_ID, INVESTOR_ID,PASSWORD);
    string msg=string(char_msg);
    LOG(INFO)<<msg;
    cout << "BROKER_ID:"<< BROKER_ID<<";INVESTOR_ID:"<<INVESTOR_ID<<";PASSWORD="<<PASSWORD <<endl;

	cerr << "hello" <<endl;
	CThostFtdcReqUserLoginField req;
    //memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.UserID, INVESTOR_ID);
	strcpy(req.Password, PASSWORD);
	int iResult = pUserApi->ReqUserLogin(&req, ++iRequestID);
    cerr << "--->>> 发送用户登录请求: " << ((iResult == 0) ? "成功" : "失败") << endl;

	

}

void CTraderSpi::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin,
		CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspUserLogin" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		// 保存会话参数;
		FRONT_ID = pRspUserLogin->FrontID;
		SESSION_ID = pRspUserLogin->SessionID;
		int iNextOrderRef = atoi(pRspUserLogin->MaxOrderRef);
		iNextOrderRef++;
		sprintf(ORDER_REF, "%d", iNextOrderRef);

		///获取当前交易日;
		cerr << "--->>> 获取当前交易日 = " << pUserApi->GetTradingDay() << endl;
		cerr << "--->>> 获取当前报单引用 = " << iNextOrderRef << endl;
		
		sprintf(char_front_id,"%d",FRONT_ID);
		sprintf(char_session_id,"%d",SESSION_ID);
		str_front_id = string(char_front_id);
		str_sessioin_id = string(char_session_id);
		//请求响应日志
		char char_msg[1024] = {'\0'};
		sprintf(char_msg,"--->>>登陆成功， 获取当前交易日 = %s,获取当前报单引用 =%s，FRONT_ID= %s,SESSION_ID=%s",pUserApi->GetTradingDay(),ORDER_REF ,char_front_id,char_session_id);
        string msg(char_msg);
        LOG(INFO)<<msg;
		//ReqQryInvestorPosition();
		///投资者结算结果确认;
        cout<<msg<<endl;
        //ReqSettlementInfoConfirm();
        //ReqQryInvestorPosition();
        ReqQryTradingAccount();
		//this->ReqOrderInsert();
		///请求查询合约
		//ReqQryInstrument();
	}
}

void CTraderSpi::ReqSettlementInfoConfirm()
{
    //确认结算
    CThostFtdcSettlementInfoConfirmField req;
    memset(&req,0,sizeof(req));
    strcpy(req.BrokerID,BROKER_ID);
    strcpy(req.InvestorID,INVESTOR_ID);
    //strcpy(req.ConfirmDate,pUserApi->GetTradingDay());buxuyao
    int iResult = pUserApi->ReqSettlementInfoConfirm(&req,++iRequestID);

    //char *cResult =  ((iResult == 0) ? "成功" : "失败") ;
    cerr << "--->>> 投资者结算结果确认: " << ((iResult == 0) ? "成功" : "失败") << endl;
//	//发送请求日志
//    char char_msg[1024]={'\0'};
//    sprintf(char_msg, "send ReqSettlementInfoConfirm:%s", ((iResult == 0) ? "success" : "failed"));
//	string msg(char_msg);
//    LogMsg logmsg;
//    logmsg.setMsg(msg);
//    logqueue.push(&logmsg);
}

void CTraderSpi::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspSettlementInfoConfirm" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
		
        string str = boosttoolsnamespace::CBoostTools::gbktoutf8("投资者结算结果确认成功，");
		str.append("brokerid=");
		str.append(pSettlementInfoConfirm->BrokerID);
		str.append(",InvestorID=");
		str.append(pSettlementInfoConfirm->InvestorID);
		str.append(",ConfirmDate=");
		str.append(pSettlementInfoConfirm->ConfirmDate);
		str.append(",ConfirmTime=");
		str.append(pSettlementInfoConfirm->ConfirmTime);
		//请求响应日志
        LOG(INFO)<<str;
        cout<<str<<endl;
		//writeStr(str);
		///请求账号信息
        //ReqQryTradingAccount();
	}
}

void CTraderSpi::ReqQryInstrument()
{
	CThostFtdcQryInstrumentField req;
	memset(&req, 0, sizeof(req));
    strcpy(req.InstrumentID,singleInstrument);
	//strcpy(req.ExchangeID,"CFFEX");
	int iResult = pUserApi->ReqQryInstrument(&req, ++iRequestID);
	cerr << "--->>> 请求查询合约: " << ((iResult == 0) ? "成功" : "失败") << endl;
}

void CTraderSpi::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspQryInstrument" << endl;
	string str;
	str.append( "--->>> OnRspQryInstrument\n");
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
        tick = pInstrument->PriceTick;
//        min_price = pInstrument->LowerLimitPrice;
//        max_price = pInstrument->UpperLimitPrice;
        querySleep();
        ReqQryInvestorPosition();
		str.append("exchangeid=");
		str.append(pInstrument->ExchangeID);
        str.append(" ,InstrumentID=");
        str.append(pInstrument->InstrumentID);
        cout<<str<<endl;
		writeStr(str);
		///请求查询合约

	}
}

void CTraderSpi::ReqQryTradingAccount()
{
	CThostFtdcQryTradingAccountField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	int iResult = pUserApi->ReqQryTradingAccount(&req, ++iRequestID);
	cerr << "--->>> 请求查询资金账户: " << ((iResult == 0) ? "成功" : "失败") << endl;

	//发送请求日志
	char char_msg[1024];
	sprintf(char_msg, "--->>> 发送请求查询资金账户 :%s", ((iResult == 0) ? "成功" : "失败"));
	string msg(char_msg);
    LOG(INFO)<<msg;
}

void CTraderSpi::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	
	cerr << "--->>> " << "OnRspQryTradingAccount" << endl;
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{
        printf("-----------------------------\n");
//        printf("投资者编号=[%s]\n",pTradingAccount->InvestorID);
        printf("资金帐号=[%s]\n",pTradingAccount->AccountID);
        printf("上次结算准备金=[%lf]\n",pTradingAccount->PreBalance);
        printf("入金金额=[%lf]\n",pTradingAccount->Deposit);
        printf("出金金额=[%lf]\n",pTradingAccount->Withdraw);
        printf("冻结的保证金=[%lf]\n",pTradingAccount->FrozenMargin);
//        printf("冻结手续费=[%lf]\n",pTradingAccount->FrozenFee);
        printf("手续费=[%lf]\n",pTradingAccount->Commission);
        printf("平仓盈亏=[%lf]\n",pTradingAccount->CloseProfit);
        printf("持仓盈亏=[%lf]\n",pTradingAccount->PositionProfit);
        printf("可用资金=[%lf]\n",pTradingAccount->Available);
//        printf("多头冻结的保证金=[%lf]\n",pTradingAccount->LongFrozenMargin);
//        printf("空头冻结的保证金=[%lf]\n",pTradingAccount->ShortFrozenMargin);
//        printf("多头保证金=[%lf]\n",pTradingAccount->LongMargin);
//        printf("空头保证金=[%lf]\n",pTradingAccount->ShortMargin);
        printf("-----------------------------\n");
        querySleep();
        ReqQryInstrument();
//        LOG(INFO)<<str;
//        cout<<str<<endl;
//		///请求查询投资者持仓
//		ReqQryInvestorPosition();
	}
}

void CTraderSpi::ReqQryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField req;
	memset(&req, 0, sizeof(req));
	strcpy(req.BrokerID, BROKER_ID);
	strcpy(req.InvestorID, INVESTOR_ID);
	//strcpy(req.InstrumentID, INSTRUMENT_ID);
	int iResult = pUserApi->ReqQryInvestorPosition(&req, ++iRequestID);

	cerr << "--->>> 请求查询投资者持仓: " << ((iResult == 0) ? "成功" : "失败") << endl;
	//发送请求日志
	char char_msg[1024];
	sprintf(char_msg, "--->>> 发送请求查询投资者持仓: %s", ((iResult == 0) ? "成功" : "失败"));
	string msg(char_msg);
    LOG(INFO)<<msg;
    //logqueue.push(&logmsg);
}
//查询投资者持仓
void CTraderSpi::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspQryInvestorPosition" << endl;
	if(!IsErrorRspInfo(pRspInfo) && pInvestorPosition){
		initpst(pInvestorPosition);
	}
	if (bIsLast && !IsErrorRspInfo(pRspInfo))
	{

		int isbeginmk = 0;
        unordered_map<string,unordered_map<string,int>>::iterator tmpit = positionmap.begin();
        if(tmpit == positionmap.end()){
            cout<<"当前无持仓信息"<<endl;
        }else{
            for(;tmpit != positionmap.end();tmpit ++){
                string str_instrument = tmpit->first;
                unordered_map<string,int> tmppst = tmpit->second;
                char char_tmp_pst[10] = {'\0'};
                char char_longyd_pst[10] = {'\0'};
                char char_longtd_pst[10] = {'\0'};
                sprintf(char_tmp_pst,"%d",tmppst["longTotalPosition"]);
                sprintf(char_longyd_pst,"%d",tmppst["longYdPosition"]);
                sprintf(char_longtd_pst,"%d",tmppst["longTdPosition"]);
                char char_tmp_pst2[10] = {'\0'};
                char char_shortyd_pst[10] = {'\0'};
                char char_shorttd_pst[10] = {'\0'};
                sprintf(char_tmp_pst2,"%d",tmppst["shortTotalPosition"]);
                sprintf(char_shortyd_pst,"%d",tmppst["shortYdPosition"]);
                sprintf(char_shorttd_pst,"%d",tmppst["shortTdPosition"]);
                if(tmppst["longYdPosition"] > 0){
                    shortPstIsClose = 2;
                    short_offset_flag = 4;
                }
                if(tmppst["shortYdPosition"] > 0){
                    longPstIsClose = 2;
                    long_offset_flag = 4;
                }
//                int longpst = tmppst["longTotalPosition"];
//                int shortpst = tmppst["shortTotalPosition"];
//                char char_longpst[12] = {'\0'};
//                char char_shortpst[12] = {'\0'};
//                sprintf(char_longpst,"%d",longpst);
//                sprintf(char_shortpst,"%d",shortpst);
                string pst_msg = "持仓结构:"+str_instrument + ",多头持仓量=" + string(char_tmp_pst) + ",今仓数量=" + string(char_longtd_pst) + ",昨仓数量=" + string(char_longyd_pst) +
                        ";空头持仓量=" + string(char_tmp_pst2) + ",今仓数量=" + string(char_shorttd_pst) + ",昨仓数量=" + string(char_shortyd_pst) ;
                cout<<pst_msg<<endl;
                LOG(INFO)<<pst_msg;
            }
        }
        //call tradeParaProcess method to set close or open
        tradeParaProcessTwo();
		cout<<"是否启动策略程序?0 否，1是"<<endl;
		cin>>isbeginmk;
		if(isbeginmk == 1){
			start_process = 1;
			isrtntradeprocess = true;
			// 初始化UserApi
			mduserapi = CThostFtdcMdApi::CreateFtdcMdApi();			// 创建UserApi
			CThostFtdcMdSpi* pUserSpi = new CMdSpi();
			mduserapi->RegisterSpi(pUserSpi);						// 注册事件类
			mduserapi->RegisterFront(MD_FRONT_ADDR);					// connect
			mduserapi->Init();
		}
	}
}
///报单录入请求
//设置了请求编号之后，在order中会自动将设置的编号作为报单回报响应里面的请求编号；同时交易所报单回报响应中的编号也是该设置编号
void CTraderSpi::ReqOrderInsert(CThostFtdcInputOrderField order)
{
	int nRequestID = ++iRequestID;
	char char_order_index[10]={'\0'};
	sprintf(char_order_index,"%d",nRequestID);
	order.RequestID = nRequestID;
	nRequestID = ++iRequestID;
	char char_query_index[10]={'\0'};
	sprintf(char_query_index,"%d",nRequestID);
	int iResult = pUserApi->ReqOrderInsert(&order, nRequestID);
	//int iResult = pUserApi->ReqOrderInsert(&order, 100);
	cerr << "--->>> 报单录入请求: " << ((iResult == 0) ? "成功" : "失败") <<" 请求编号："<<iRequestID<< endl;
	//保存对冲报单请求编号，用于确认对冲报单
	mapRequestid[nRequestID] = -1;
	//记录对冲报单录入信息
	//saveInvestorOrderInsertHedge(&order,"d:\\test\\reqOrderInsertHedge.txt");
	string msg = "order_requestid=" + string(char_order_index) + ";orderinsert_requestid=" + string(char_query_index);
    LOG(INFO)<<msg;
}
//1、CTP响应组成对冲记录，以requestid为主键
//2、交易所响应进行修改，把requestid主键修改为ordersysid
void CTraderSpi::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    cout<<"------->>OnRspOrderInsert"<<endl;
	if (pRspInfo != NULL && IsErrorRspInfo(pRspInfo))
	{
		cerr << "--->>> " << "OnRspOrderInsert" << "响应请求编号："<<nRequestID<< " CTP回报请求编号"<<pInputOrder->RequestID<<endl;
		string sInputOrderInfo = getInvestorOrderInsertInfo(pInputOrder);
		string sResult;
        char cErrorID[10]={'\0'};
//		itoa(pRspInfo->ErrorID,cErrorID,10);
        sprintf(cErrorID,"%d",pRspInfo->ErrorID);
		char cRequestid[100];
		sprintf(cRequestid,"%d",nRequestID);
		char ctpRequestId[100];
		sprintf(ctpRequestId,"%d",pInputOrder->RequestID);
		sResult.append("CTP报单回报信息--->>> ErrorID=");
		sResult.append(cErrorID);
		sResult.append(", ErrorMsg=");
        sResult.append(boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg));
		sInputOrderInfo.append(sResult);
		sInputOrderInfo.append("响应请求编号nRequestID：");
		sInputOrderInfo.append(cRequestid);
		sInputOrderInfo.append( " CTP回报请求编号pInputOrder->RequestID:");
		sInputOrderInfo.append(ctpRequestId);
		//记录失败的对冲报单
        LOG(INFO)<<sInputOrderInfo;
    }
	string ordreInfo = getInvestorOrderInsertInfo(pInputOrder);
    LOG(INFO)<<ordreInfo;
	//saveRspOrderInsertInfo(pInputOrder);
}
///请求查询报单响应
void CTraderSpi::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) 
{
    cout<<"------->OnRspQryOrder>"<<endl;
}

///报单录入错误回报
void CTraderSpi::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    cout<<"------->OnErrRtnOrderInsert>"<<endl;
   // string sInputOrderInfo = getInvestorOrderInsertInfo(pInputOrder);
    if (pRspInfo != NULL && IsErrorRspInfo(pRspInfo))
    {
        string sInputOrderInfo = getInvestorOrderInsertInfo(pInputOrder);
        string sResult;
        char cErrorID[10]={'\0'};
//		itoa(pRspInfo->ErrorID,cErrorID,10);
        sprintf(cErrorID,"%d",pRspInfo->ErrorID);
        char cRequestid[100];
        char ctpRequestId[100];
        sprintf(ctpRequestId,"%d",pInputOrder->RequestID);
        sResult.append("CTP报单回报信息--->>> ErrorID=");
        sResult.append(cErrorID);
        sResult.append(", ErrorMsg=");
        sResult.append(boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg));
        sInputOrderInfo.append(sResult);
        sInputOrderInfo.append("响应请求编号nRequestID：");
        sInputOrderInfo.append(cRequestid);
        sInputOrderInfo.append( " CTP回报请求编号pInputOrder->RequestID:");
        sInputOrderInfo.append(ctpRequestId);
        //记录失败的对冲报单
        LOG(INFO)<<sInputOrderInfo;
    }
}

void CTraderSpi::ReqOrderAction(CThostFtdcOrderField *pOrder)
{
	static bool ORDER_ACTION_SENT = false;		//是否发送了报单
	if (ORDER_ACTION_SENT)
		return;

	CThostFtdcInputOrderActionField req;
	memset(&req, 0, sizeof(req));
	///经纪公司代码
	strcpy(req.BrokerID, pOrder->BrokerID);
	///投资者代码
	strcpy(req.InvestorID, pOrder->InvestorID);
	///报单操作引用
//	TThostFtdcOrderActionRefType	OrderActionRef;
	///报单引用
	strcpy(req.OrderRef, pOrder->OrderRef);
	///请求编号
//	TThostFtdcRequestIDType	RequestID;
	///前置编号
	req.FrontID = FRONT_ID;
	///会话编号
	req.SessionID = SESSION_ID;
	///交易所代码
//	TThostFtdcExchangeIDType	ExchangeID;
	///报单编号
//	TThostFtdcOrderSysIDType	OrderSysID;
	///操作标志
	req.ActionFlag = THOST_FTDC_AF_Delete;
	///价格
//	TThostFtdcPriceType	LimitPrice;
	///数量变化
//	TThostFtdcVolumeType	VolumeChange;
	///用户代码
//	TThostFtdcUserIDType	UserID;
	///合约代码
	strcpy(req.InstrumentID, pOrder->InstrumentID);

	int iResult = pUserApi->ReqOrderAction(&req, ++iRequestID);
	cerr << "--->>> 报单操作请求: " << ((iResult == 0) ? "成功" : "失败") << endl;
	ORDER_ACTION_SENT = true;
}

void CTraderSpi::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspOrderAction" << endl;
	IsErrorRspInfo(pRspInfo);
}

///报单通知
void CTraderSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    /////买
//#define THOST_FTDC_D_Buy '0'
///卖
//#define THOST_FTDC_D_Sell '1'
	cerr << "--->>> " << "OnRtnOrder"  << "交易所回报请求编号："<<pOrder->RequestID<<endl;
//    char orderSysID[25];
//    strcpy(orderSysID,pOrder->OrderSysID);
    string msg = "OnRtnOrder:";
    msg.append(getRtnOrder(pOrder));
    LOG(INFO)<<msg;
    ///撤单
    // #define THOST_FTDC_OST_Canceled '5'
    char c_order_status[5] = "5";
    int isActionOrder = strcmp(&pOrder->OrderStatus,c_order_status);
    if(isActionOrder != 0){// not action order,do not assemble order
        cout<<"normal onRtnOrder,do not assemble order"<<endl;
        return;
    }
    CThostFtdcInputOrderField* order = assamble(pOrder);
    //order will be insert,which store in askordermap or bidordermap
    int32_t tmp_orderID = orderID;
    orderID += 1;

    char c_dir_buy[5] = "0";
    char c_dir_sell[5] = "1";
    //order direction
    int dir_buy = strcmp(&order->Direction,c_dir_buy);
    int dir_sell = strcmp(&order->Direction,c_dir_sell);

    //volume which order been action
    int32_t tmp_vol = order->VolumeTotalOriginal;
    //order price
    double tmp_price = order->LimitPrice;

    boost::recursive_mutex::scoped_lock SLock(actionOrderMTX);
    if(dir_buy == 0){//buy direction
        unordered_map<double,unordered_map<string,int32_t>>::iterator it = delayedBidOrderMap.find(tmp_price);
        if(it == delayedBidOrderMap.end()){//new create
            unordered_map<string,int32_t> tmp_order_map;
            tmp_order_map["orderID"] = tmp_orderID;
            tmp_order_map["volume"] = tmp_vol;
            delayedBidOrderMap[tmp_price] = tmp_order_map;
            allDelayedOrder[tmp_orderID] = order;
        }else{//update data
            unordered_map<string,int32_t> bid_order_map = it->second;
            //modify order volume
            unordered_map<string,int32_t>::iterator bidVol_it = bid_order_map.find("volume");
            bidVol_it->second = bidVol_it->second + tmp_vol;
        }
    }else if(dir_sell == 0){//sell direction order action
        unordered_map<double,unordered_map<string,int32_t>>::iterator it = delayedAskOrderMap.find(tmp_price);
        if(it == delayedAskOrderMap.end()){//new create ask order
            unordered_map<string,int32_t> tmp_order_map;
            tmp_order_map["orderID"] = tmp_orderID;
            tmp_order_map["volume"] = tmp_vol;
            delayedAskOrderMap[tmp_price] = tmp_order_map;
            allDelayedOrder[tmp_orderID] = order;
        }else{
            //modify order volume
            it->second["volume"] = it->second["volume"] + tmp_vol;
        }
    }

}

///成交通知
void CTraderSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	ret ++;
	cerr << "--->>> " << "OnRtnTrade"  << endl;
	//处理持仓
    int64_t start = GetSysTimeMicros();
    //auto start = boost::chrono::high_resolution_clock::now();
    char* ordersysid = pTrade->OrderSysID;
    string str_ordersysid = boost::trim_copy(string(ordersysid));
    if(seq_map_ordersysid.find(str_ordersysid) != seq_map_ordersysid.end()){
        string tmpstr = "error:查询不到ordersysid="+str_ordersysid+" 的报单信息";
         LOG(INFO)<<tmpstr;
    }else if(isrtntradeprocess){
        //根据ordersysid查询orderinsertkey
//        unordered_map<string,string>::iterator tmpit = seq_map_ordersysid.find(str_ordersysid);
//		string orderinsertkey = tmpit->second;
//		if(orderinsertkey.size() != 0){
//            unordered_map<string,unordered_map<string,int64_t>>::iterator tmptrade = seq_map_orderref.find(tmpit->second);
//            unordered_map<string,int64_t> timemap = tmptrade->second;//遍历报单的相关时间差
//            timemap["exchange_rtntrade"] = start;
//            unordered_map<string,int64_t>::iterator mapit = timemap.begin();
//			string str_time_msg;
//			while(mapit!=timemap.end()){
//				string key = mapit->first;
////                stringstream ss;
////                ss<<boost::chrono::duration<double>(start -mapit->second).count();
//                char char_time[21] = {'\0'};
//                sprintf(char_time,"%d",mapit->second);
//                string str_time = ss.str();
//				str_time_msg.append(key + "=" +str_time +";" );
//				mapit ++ ;
//			}
//             LOG(INFO)<<str_time_msg;
//		}
    }
	processtrade(pTrade);
    int64_t end1 = GetSysTimeMicros();
	string tradeInfo = storeInvestorTrade(pTrade);
    LOG(INFO)<<tradeInfo;
    int64_t end2 = GetSysTimeMicros();
 //   string msg = "处理成交花费:" + string(GetDiffTime(end1,start));
 //   LOG(INFO)<<msg;
    //recordRunningMsg(msg);
//    recordRunningMsg(tradeInfo);
    //msg = "记录成交花费:" + string(GetDiffTime(end2,end1));
    //recordRunningMsg(msg);
//    logqueue.push(&logmsg);
}

void CTraderSpi::OnHeartBeatWarning(int nTimeLapse)
{
	cerr << "--->>> " << "OnHeartBeatWarning" << endl;
	cerr << "--->>> nTimerLapse = " << nTimeLapse << endl;
}

void CTraderSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	cerr << "--->>> " << "OnRspError" << endl;
	string sResult;
	char cErrorID[100] ;
	char cRequestid[100];
//	itoa(pRspInfo->ErrorID,cErrorID,10);
    sprintf(cErrorID,"%d",pRspInfo->ErrorID);
	sprintf(cRequestid,"%d",nRequestID);
	sResult.append("CTP错误回报--->>> ErrorID=");
	sResult.append(cErrorID);
	sResult.append(", ErrorMsg=");
    sResult.append( boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg));
	sResult.append(",requestid=");
	sResult.append(cRequestid);
	//记录错误回报报单信息
	//write(sResult,"d:\\test\\log.txt");
    cout<<sResult<<endl;
//    LogMsg logmsg;
//    logmsg.setMsg(sResult);
//    logqueue.push(&logmsg);
	IsErrorRspInfo(pRspInfo);
}

bool CTraderSpi::IsErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
    cout<<"--->>IsErrorRspInfo"<<endl;
    CThostFtdcRspInfoField *pRspInfo2 = (CThostFtdcRspInfoField*)pRspInfo;
	// 如果ErrorID != 0, 说明收到了错误的响应
    printf("%p\n",pRspInfo);
    char c_pid[20];
    sprintf(c_pid,"%p",pRspInfo);
    cout<<c_pid<<endl;
    int d = strcmp(c_pid,"0x31");
    printf("%d\n",d);
    if(d == 0){
        cout<<"error rspinfo"<<endl;
        return false;
    }
    bool bResult = ((pRspInfo) && (pRspInfo2->ErrorID != 0));
    cout<<"iserror"<<bResult<<" "<< pRspInfo<<endl;
//    string errmsg = boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg);

    char char_msg[1024]={'\0'};
	if (bResult){
        string errmsg =boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg);
		if(pRspInfo->ErrorID == 22){//重复的ref
			orderref += 1000;
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<",orderref增加="<<orderref<<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s,orderref增加=%d",pRspInfo->ErrorID,  errmsg,orderref);

        }else if(pRspInfo->ErrorID == 31){//资金不足
			isclose = 2;
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<",isclose平仓开仓方式修改为:"<<isclose<<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s,isclose平仓开仓方式修改为:%d",pRspInfo->ErrorID,  errmsg,isclose);
        }else if(pRspInfo->ErrorID == 30){//平仓量超过持仓量
			isclose = 1;
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<",isclose平仓开仓方式修改为:"<<isclose<<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s,isclose平仓开仓方式修改为:%d",pRspInfo->ErrorID, errmsg,isclose);
        }else if(pRspInfo->ErrorID == 51){//平昨仓位不足
			offset_flag = 3;
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<",平仓方式修改为平今:"<<offset_flag<<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s,平仓方式修改为平今:%d",pRspInfo->ErrorID,errmsg,offset_flag);
        }else if(pRspInfo->ErrorID == 50){//平今仓位不足
			isclose = 1;
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<",isclose平仓开仓方式修改为:"<<isclose<<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s,isclose平仓开仓方式修改为:%d",pRspInfo->ErrorID,errmsg,isclose);
        }else if(pRspInfo->ErrorID == 3){//不合法的登录
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << errmsg <<endl;
            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s",pRspInfo->ErrorID,errmsg);
        }else{
            cerr << "--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg) <<endl;

            sprintf(char_msg, "--->>> ErrorID=%d,ErrorMsg=%s",pRspInfo->ErrorID,  boosttoolsnamespace::CBoostTools::gbktoutf8(pRspInfo->ErrorMsg));
        }
        string msg(char_msg);
        cout<<"----------"<<char_msg<<endl;
        LOG(INFO)<<msg;
	}
	return bResult;
}

bool CTraderSpi::IsMyOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->FrontID == FRONT_ID) &&
			(pOrder->SessionID == SESSION_ID) &&
			(strcmp(pOrder->OrderRef, ORDER_REF) == 0));
}

bool CTraderSpi::IsTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing) &&
			(pOrder->OrderStatus != THOST_FTDC_OST_Canceled) &&
			(pOrder->OrderStatus != THOST_FTDC_OST_AllTraded));
}

void writeStr(string str){
	char cw[10240]="";
	str.copy(cw,str.size(),0);
	ofstream in;
	string pa = logpath + "com.txt";
	in.open(pa,ios::app); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
	in<<cw<<endl;
	in.close();//关闭文件
}
void writeTrade(string str){
	char cw[10240]="";
	str.copy(cw,str.size(),0);
	ofstream in;
	in.open("d:\\test\\trade.txt",ios::app); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
	in<<cw<<endl;
	in.close();//关闭文件
}
void write(string str,string filepath){
	char cw[10240]="";
	str.copy(cw,str.size(),0);
	ofstream in;
	in.open(filepath,ios::app); //ios::trunc表示在打开文件前将文件清空,由于是写入,文件不存在则创建
	in<<cw<<endl;
	in.close();//关闭文件
}
//将报单回报响应信息写入文件保存
void saveRspOrderInsertInfo(CThostFtdcInputOrderField *pInputOrder)
{
	saveInvestorOrderInsertHedge(pInputOrder, "d:\\test\\rsporderinsert.txt");
}
//获取交易所回报响应
string getRtnOrder(CThostFtdcOrderField *pOrder)
{
	///经纪公司代码
	char	*BrokerID = pOrder->BrokerID;
	///投资者代码
	char	*InvestorID = pOrder->InvestorID;
	///合约代码
	char	*InstrumentID = pOrder->InstrumentID;
	///报单引用
	char	*OrderRef = pOrder->OrderRef;
	///用户代码
	TThostFtdcUserIDType	UserID;
	///报单价格条件
	TThostFtdcOrderPriceTypeType	OrderPriceType;
	///买卖方向
	TThostFtdcDirectionType	Direction = pOrder->Direction;
	///组合开平标志
	TThostFtdcCombOffsetFlagType	CombOffsetFlag;
	///组合投机套保标志
	TThostFtdcCombHedgeFlagType	CombHedgeFlag;
	///价格
	TThostFtdcPriceType	LimitPrice;
	///数量
	TThostFtdcVolumeType	VolumeTotalOriginal;
	///有效期类型
	TThostFtdcTimeConditionType	TimeCondition;
	///GTD日期
	TThostFtdcDateType	GTDDate;
	///成交量类型
	TThostFtdcVolumeConditionType	VolumeCondition;
	///最小成交量
	TThostFtdcVolumeType	MinVolume;
	///触发条件
	TThostFtdcContingentConditionType	ContingentCondition;
	///止损价
	TThostFtdcPriceType	StopPrice;
	///强平原因
	TThostFtdcForceCloseReasonType	ForceCloseReason;
	///自动挂起标志
	TThostFtdcBoolType	IsAutoSuspend;
	///业务单元
	TThostFtdcBusinessUnitType	BusinessUnit;
	///请求编号
	TThostFtdcRequestIDType	RequestID = pOrder->RequestID;
	char cRequestId[100];
	sprintf(cRequestId,"%d",RequestID);
	///本地报单编号
	char	*OrderLocalID = pOrder->OrderLocalID;
	///交易所代码
	TThostFtdcExchangeIDType	ExchangeID;
	///会员代码
	TThostFtdcParticipantIDType	ParticipantID;
	///客户代码
	char	*ClientID = pOrder->ClientID;
	///合约在交易所的代码
	TThostFtdcExchangeInstIDType	ExchangeInstID;
	///交易所交易员代码
	TThostFtdcTraderIDType	TraderID;
	///安装编号
	TThostFtdcInstallIDType	InstallID;
	///报单提交状态
	char	OrderSubmitStatus = pOrder->OrderSubmitStatus;
	char cOrderSubmitStatus[] = {OrderSubmitStatus,'\0'};
	//sprintf(cOrderSubmitStatus,"%s",OrderSubmitStatus);
	///报单状态
	TThostFtdcOrderStatusType	OrderStatus = pOrder->OrderStatus;
	char cOrderStatus[] = {OrderStatus,'\0'};
	//sprintf(cOrderStatus,"%s",OrderStatus);
	///报单提示序号
	TThostFtdcSequenceNoType	NotifySequence;
	///交易日
	TThostFtdcDateType	TradingDay;
	///结算编号
	TThostFtdcSettlementIDType	SettlementID;
	///报单编号
	char	*OrderSysID = pOrder->OrderSysID;
    string str_ordersysid = boost::trim_copy(string(OrderSysID));

	///报单来源
	TThostFtdcOrderSourceType	OrderSource;
	///报单类型
	TThostFtdcOrderTypeType	OrderType;
	///今成交数量
	TThostFtdcVolumeType	VolumeTraded = pOrder->VolumeTraded;
	char cVolumeTraded[100];
	sprintf(cVolumeTraded,"%d",VolumeTraded);
	///剩余数量
	TThostFtdcVolumeType	VolumeTotal = pOrder->VolumeTotal;
	char iVolumeTotal[100];
	sprintf(iVolumeTotal,"%d",VolumeTotal);
	///报单日期
	TThostFtdcDateType	InsertDate;
	///委托时间
	TThostFtdcTimeType	InsertTime;
	///激活时间
	TThostFtdcTimeType	ActiveTime;
	///挂起时间
	TThostFtdcTimeType	SuspendTime;
	///最后修改时间
	TThostFtdcTimeType	UpdateTime;
	///撤销时间
	TThostFtdcTimeType	CancelTime;
	///最后修改交易所交易员代码
	TThostFtdcTraderIDType	ActiveTraderID;
	///结算会员编号
	TThostFtdcParticipantIDType	ClearingPartID;
	///序号
	TThostFtdcSequenceNoType	SequenceNo;
	///前置编号
	TThostFtdcFrontIDType	FrontID;
	///会话编号
	TThostFtdcSessionIDType	SessionID;
	///用户端产品信息
	TThostFtdcProductInfoType	UserProductInfo;
	///状态信息
	char	*StatusMsg = pOrder->StatusMsg;
	///用户强评标志
	TThostFtdcBoolType	UserForceClose;
	///操作用户代码
	TThostFtdcUserIDType	ActiveUserID;
	///经纪公司报单编号
	TThostFtdcSequenceNoType	BrokerOrderSeq;

	char char_ordref[21] = {'\0'};
	sprintf(char_ordref,"%s",pOrder->OrderRef);
	string str_orderk = str_front_id+str_sessioin_id+string(char_ordref);
    //处理报单回报
    boost::recursive_mutex::scoped_lock SLock(order_mtx);
	if(seq_map_orderref.find(str_orderk)==seq_map_orderref.end()){
        string tmp_msg = "error:查询不到"+str_orderk+"对应的报单信息";
        LOG(INFO)<<tmp_msg;
	}else{
        unordered_map<string,unordered_map<string,int64_t>>::iterator tmpit = seq_map_orderref.find(str_orderk);
        int64_t ist_time = tmpit->second["ist_time"];
        int64_t tmpendtime = GetSysTimeMicros();
        //auto tmpendtime = boost::chrono::high_resolution_clock::now();
		//没有对应关系
		if(strlen(OrderSysID)  != 0 && seq_map_ordersysid.find(str_ordersysid)==seq_map_ordersysid.end()){
			seq_map_ordersysid[str_ordersysid] = str_orderk;
            LOG(INFO)<<"ordersysid="+str_ordersysid;
		}
        double elapse = boost::chrono::duration<double>(tmpendtime - ist_time).count();
		if(strlen(OrderSysID) == 0){//CTP返回
            tmpit->second["ctp_rtnorder"] = tmpendtime;
		}else if(strcmp(cOrderStatus,"3") == 0){//未成交
            tmpit->second["exchange_rtnorder_untrade"] = tmpendtime;
		}else if(strcmp(cOrderStatus,"5") == 0){//已经撤单
            tmpit->second["exchange_rtnorder_action"] =tmpendtime;
		}else if(strcmp(cOrderStatus,"0") == 0){//全部成交
            tmpit->second["exchange_rtnorder_alltrade"] = tmpendtime;
		}
	}
	string info;
	info.append(BrokerID);info.append("\t");
	info.append(InvestorID);info.append("\t");
	info.append(InstrumentID);info.append("\t");
	info.append(OrderRef);info.append("\t");
	info.append(cRequestId);info.append("\t");
	info.append(ClientID);info.append("\t");
	info.append(OrderSysID);info.append("\t");
	info.append(cVolumeTraded);info.append("\t");
	info.append(iVolumeTotal);info.append("\t");
	info.append(StatusMsg);info.append("\t");
	info.append(cOrderSubmitStatus);info.append("\t");
	info.append(cOrderStatus);info.append("\t");
	return info;
}

//将交易所报单回报响应写入文件保存
void saveRtnOrder(CThostFtdcOrderField *pOrder)
{
	string info = getRtnOrder(pOrder);
	write(info,"d:\\test\\exchangeRtnOrderInsert.txt");
}
//将投资者持仓信息写入文件保存
int storeInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition)
{
	
	///合约代码
	char	*InstrumentID = pInvestorPosition->InstrumentID;
	///经纪公司代码
	char	*BrokerID = pInvestorPosition->BrokerID;
	///投资者代码
	char	*InvestorID = pInvestorPosition->InvestorID;
	///持仓多空方向
	TThostFtdcPosiDirectionType	dir = pInvestorPosition->PosiDirection;
	char PosiDirection[] = {dir,'\0'};
	///投机套保标志
	TThostFtdcHedgeFlagType	flag = pInvestorPosition->HedgeFlag;
	char HedgeFlag[] = {flag,'\0'};
	///持仓日期
	TThostFtdcPositionDateType	positionDate = pInvestorPosition->PositionDate;
	char PositionDate[] = {positionDate,'\0'};
	///上日持仓
	TThostFtdcVolumeType	ydPosition = pInvestorPosition->YdPosition;
	char YdPosition[100];
	sprintf(YdPosition,"%d",ydPosition);
	///今日持仓
	TThostFtdcVolumeType	position = pInvestorPosition->Position;
	char Position[100];
	sprintf(Position,"%d",position);
	///多头冻结
	TThostFtdcVolumeType	LongFrozen = pInvestorPosition->LongFrozen;
	///空头冻结
	TThostFtdcVolumeType	ShortFrozen = pInvestorPosition->ShortFrozen;
	///开仓冻结金额
	TThostFtdcMoneyType	LongFrozenAmount = pInvestorPosition->LongFrozenAmount;
	///开仓冻结金额
	TThostFtdcMoneyType	ShortFrozenAmount = pInvestorPosition->ShortFrozenAmount;
	///开仓量
	TThostFtdcVolumeType	openVolume = pInvestorPosition->OpenVolume;
	char OpenVolume[100] ;
	sprintf(OpenVolume,"%d",openVolume);
	///平仓量
	TThostFtdcVolumeType	closeVolume = pInvestorPosition->CloseVolume;
	char CloseVolume[100];
	sprintf(CloseVolume,"%d",closeVolume);
	///开仓金额
	TThostFtdcMoneyType	OpenAmount = pInvestorPosition->OpenAmount;
	///平仓金额
	TThostFtdcMoneyType	CloseAmount = pInvestorPosition->CloseAmount;
	///持仓成本
	TThostFtdcMoneyType	positionCost = pInvestorPosition->PositionCost;
	char PositionCost[100];
	sprintf(PositionCost,"%f",positionCost);
	///上次占用的保证金
	TThostFtdcMoneyType	PreMargin = pInvestorPosition->PreMargin;
	///占用的保证金
	TThostFtdcMoneyType	UseMargin = pInvestorPosition->UseMargin;
	///冻结的保证金
	TThostFtdcMoneyType	FrozenMargin = pInvestorPosition->FrozenMargin;
	///冻结的资金
	TThostFtdcMoneyType	FrozenCash = pInvestorPosition->FrozenCash;
	///冻结的手续费
	TThostFtdcMoneyType	FrozenCommission = pInvestorPosition->FrozenCommission;
	///资金差额
	TThostFtdcMoneyType	CashIn = pInvestorPosition->CashIn;
	///手续费
	TThostFtdcMoneyType	Commission = pInvestorPosition->Commission;
	///平仓盈亏
	TThostFtdcMoneyType	CloseProfit = pInvestorPosition->CloseProfit;
	///持仓盈亏
	TThostFtdcMoneyType	PositionProfit = pInvestorPosition->PositionProfit;
	///上次结算价
	TThostFtdcPriceType	preSettlementPrice = pInvestorPosition->PreSettlementPrice;
	char PreSettlementPrice[100];
	sprintf(PreSettlementPrice,"%f",preSettlementPrice);
	///本次结算价
	TThostFtdcPriceType	SettlementPrice = pInvestorPosition->PreSettlementPrice;
	///交易日
	char	*TradingDay = pInvestorPosition->TradingDay;
	///结算编号
	TThostFtdcSettlementIDType	SettlementID;
	///开仓成本
	TThostFtdcMoneyType	openCost = pInvestorPosition->OpenCost;
	char OpenCost[100] ;
	sprintf(OpenCost,"%f",openCost);
	///交易所保证金
	TThostFtdcMoneyType	exchangeMargin = pInvestorPosition->ExchangeMargin;
	char ExchangeMargin[100];
	sprintf(ExchangeMargin,"%f",exchangeMargin);
	///组合成交形成的持仓
	TThostFtdcVolumeType	CombPosition;
	///组合多头冻结
	TThostFtdcVolumeType	CombLongFrozen;
	///组合空头冻结
	TThostFtdcVolumeType	CombShortFrozen;
	///逐日盯市平仓盈亏
	TThostFtdcMoneyType	CloseProfitByDate = pInvestorPosition->CloseProfitByDate;
	///逐笔对冲平仓盈亏
	TThostFtdcMoneyType	CloseProfitByTrade = pInvestorPosition->CloseProfitByTrade;
	///今日持仓
	TThostFtdcVolumeType	todayPosition = pInvestorPosition->TodayPosition;
	char TodayPosition[100] ;
	sprintf(TodayPosition,"%d",todayPosition);
	///保证金率
	TThostFtdcRatioType	marginRateByMoney = pInvestorPosition->MarginRateByMoney;
	char MarginRateByMoney[100];
	sprintf(MarginRateByMoney,"%f",marginRateByMoney);
	///保证金率(按手数)
	TThostFtdcRatioType	marginRateByVolume = pInvestorPosition->MarginRateByVolume;
	char MarginRateByVolume[100];
	sprintf(MarginRateByVolume,"%f",marginRateByVolume);
	string sInvestorInfo;
	sInvestorInfo.append("InstrumentID=").append(InstrumentID);sInvestorInfo.append("\t");
	sInvestorInfo.append("BrokerID=").append(BrokerID);sInvestorInfo.append("\t");
	sInvestorInfo.append("InvestorID=").append(InvestorID);sInvestorInfo.append("\t");
	sInvestorInfo.append("PosiDirection=").append(PosiDirection);sInvestorInfo.append("\t");
	sInvestorInfo.append("HedgeFlag=").append(HedgeFlag);sInvestorInfo.append("\t");
	sInvestorInfo.append("PositionDate=").append(PositionDate);sInvestorInfo.append("\t");
	sInvestorInfo.append("YdPosition=").append(YdPosition);sInvestorInfo.append("\t");
	sInvestorInfo.append("Position=").append(Position);sInvestorInfo.append("\t");
	sInvestorInfo.append("OpenVolume=").append(OpenVolume);sInvestorInfo.append("\t");
	sInvestorInfo.append("CloseVolume=").append(CloseVolume);sInvestorInfo.append("\t");
	sInvestorInfo.append("PositionCost=").append(PositionCost);sInvestorInfo.append("\t");
	sInvestorInfo.append("PreSettlementPrice=").append(PreSettlementPrice);sInvestorInfo.append("\t");

	sInvestorInfo.append("TradingDay=").append(TradingDay);sInvestorInfo.append("\t");
	sInvestorInfo.append("OpenCost=").append(OpenCost);sInvestorInfo.append("\t");
	sInvestorInfo.append("ExchangeMargin=").append(ExchangeMargin);sInvestorInfo.append("\t");
	sInvestorInfo.append("TodayPosition=").append(TodayPosition);sInvestorInfo.append("\t");
	sInvestorInfo.append("MarginRateByMoney=").append(MarginRateByMoney);sInvestorInfo.append("\t");
	sInvestorInfo.append("MarginRateByVolume=").append(MarginRateByVolume);sInvestorInfo.append("\t");
    LOG(INFO)<<sInvestorInfo;
	return 0;
}
//初始化持仓信息
void initpst(CThostFtdcInvestorPositionField *pInvestorPosition)
{
    boost::recursive_mutex::scoped_lock SLock(pst_mtx);
	///合约代码
	char	*InstrumentID = pInvestorPosition->InstrumentID;
	string str_instrumentid= string(InstrumentID);
	///持仓多空方向
	TThostFtdcPosiDirectionType	dir = pInvestorPosition->PosiDirection;
	char PosiDirection[] = {dir,'\0'};
	///投机套保标志
	TThostFtdcHedgeFlagType	flag = pInvestorPosition->HedgeFlag;
	char HedgeFlag[] = {flag,'\0'};
	///上日持仓
	TThostFtdcVolumeType	ydPosition = pInvestorPosition->YdPosition;
	char YdPosition[100];
	sprintf(YdPosition,"%d",ydPosition);
	///今日持仓
	TThostFtdcVolumeType	position = pInvestorPosition->Position;
	char Position[100];
	sprintf(Position,"%d",position);
	
	string str_dir = string(PosiDirection);

	///持仓日期
	TThostFtdcPositionDateType	positionDate = pInvestorPosition->PositionDate;
	char PositionDate[] = {positionDate,'\0'};
	if(positionmap.find(str_instrumentid) == positionmap.end()){//暂时没有处理，不需要考虑多空方向
        unordered_map<string,int> tmpmap;
		if("2" == str_dir){//买 
			//多头
			tmpmap["longTotalPosition"] = position;
			//空头
			tmpmap["shortTotalPosition"] = 0;
		}else if("3" == str_dir){//空
			//空头
			tmpmap["shortTotalPosition"] = position;
			tmpmap["longTotalPosition"] = 0;
		}else{
			cout<<InstrumentID<<";error:持仓类型无法判断PosiDirection="<<str_dir<<endl;
			exit;
		}
		positionmap[str_instrumentid] = tmpmap;
	}else{
        unordered_map<string,unordered_map<string,int>>::iterator tmpmap  = positionmap.find(str_instrumentid);
		//对应的反方向应该已经存在，这里后续需要确认
		if("2" == str_dir){//多 
			//多头
			tmpmap->second["longTotalPosition"] = position + tmpmap->second["longTotalPosition"];
		}else if("3" == str_dir){//空
			//空头
			tmpmap->second["shortTotalPosition"] = position + tmpmap->second["shortTotalPosition"] ;
		}else{
			cout<<InstrumentID<<";error:持仓类型无法判断PosiDirection="<<str_dir<<endl;
			exit;
		}
	}
	storeInvestorPosition(pInvestorPosition);
}
////将投资者对冲报单信息写入文件保存
void saveInvestorOrderInsertHedge(CThostFtdcInputOrderField *order,string filepath)
{
	string ordreInfo = getInvestorOrderInsertInfo(order);
	//cerr << "--->>> 写入对冲信息开始" << endl;
	write(ordreInfo, filepath);
	//cerr << "--->>> 写入对冲信息结束" << endl;
}

//提取投资者报单信息
string getInvestorOrderInsertInfo(CThostFtdcInputOrderField *order)
{
	///经纪公司代码
	char	*BrokerID = order->BrokerID;
	///投资者代码
	char	*InvestorID = order->InvestorID;
	///合约代码
	char	*InstrumentID = order->InstrumentID;
	///报单引用
	char	*OrderRef = order->OrderRef;
	///用户代码
	char	*UserID = order->UserID;
	///报单价格条件
	char	OrderPriceType = order->OrderPriceType;
	///买卖方向
	char	Direction[] = {order->Direction,'\0'};
	///组合开平标志
	char	*CombOffsetFlag =order->CombOffsetFlag;
	///组合投机套保标志
	char	*CombHedgeFlag = order->CombHedgeFlag;
	///价格
	TThostFtdcPriceType	limitPrice = order->LimitPrice;
	char LimitPrice[100];
	sprintf(LimitPrice,"%f",limitPrice);
	///数量
	TThostFtdcVolumeType	volumeTotalOriginal = order->VolumeTotalOriginal;
	char VolumeTotalOriginal[100];
	sprintf(VolumeTotalOriginal,"%d",volumeTotalOriginal);
	///有效期类型
	TThostFtdcTimeConditionType	TimeCondition = order->TimeCondition;
	///GTD日期
	//TThostFtdcDateType	GTDDate = order->GTDDate;
	///成交量类型
	TThostFtdcVolumeConditionType	VolumeCondition[] = {order->VolumeCondition,'\0'};
	///最小成交量
	TThostFtdcVolumeType	MinVolume = order->MinVolume;
	///触发条件
	TThostFtdcContingentConditionType	ContingentCondition = order->ContingentCondition;
	///止损价
	TThostFtdcPriceType	StopPrice = order->StopPrice;
	///强平原因
	TThostFtdcForceCloseReasonType	ForceCloseReason = order->ForceCloseReason;
	///自动挂起标志
	TThostFtdcBoolType	IsAutoSuspend = order->IsAutoSuspend;
	///业务单元
	//TThostFtdcBusinessUnitType	BusinessUnit = order->BusinessUnit;
	///请求编号
	TThostFtdcRequestIDType	requestID = order->RequestID;
	char RequestID[100];
	sprintf(RequestID,"%d",requestID);
	///用户强评标志
	TThostFtdcBoolType	UserForceClose = order->UserForceClose;

	string ordreInfo;
	ordreInfo.append("BrokerID=").append(BrokerID);ordreInfo.append("\t");
	ordreInfo.append("InvestorID=").append(InvestorID);ordreInfo.append("\t");
	ordreInfo.append("InstrumentID=").append(InstrumentID);ordreInfo.append("\t");
	ordreInfo.append("OrderRef=").append(OrderRef);ordreInfo.append("\t");
	ordreInfo.append("UserID=").append(UserID);ordreInfo.append("\t");
	ordreInfo.append("Direction").append(Direction);ordreInfo.append("\t");
	ordreInfo.append("CombOffsetFlag").append(CombOffsetFlag);ordreInfo.append("\t");
	ordreInfo.append("CombHedgeFlag").append(CombHedgeFlag);ordreInfo.append("\t");
	ordreInfo.append("LimitPrice").append(LimitPrice);ordreInfo.append("\t");
	ordreInfo.append("VolumeTotalOriginal").append(VolumeTotalOriginal);ordreInfo.append("\t");
	ordreInfo.append("VolumeCondition").append(VolumeCondition);ordreInfo.append("\t");
	ordreInfo.append("RequestID").append(RequestID);ordreInfo.append("\t");
	return ordreInfo;
}
//将投资者成交信息写入文件保存
string storeInvestorTrade(CThostFtdcTradeField *pTrade)
{
	string tradeInfo;
	///买卖方向
	TThostFtdcDirectionType	direction = pTrade->Direction;
	char Direction[]={direction,'\0'};
	//sprintf(Direction,"%s",direction);
	///开平标志
	TThostFtdcOffsetFlagType	offsetFlag = pTrade->OffsetFlag;
	char OffsetFlag[]={offsetFlag,'\0'};
	//sprintf(OffsetFlag,"%s",offsetFlag);
	///经纪公司代码
	char	*BrokerID = pTrade->BrokerID;
	///投资者代码
	char	*InvestorID = pTrade->InvestorID;
	///合约代码
	char	*InstrumentID =pTrade->InstrumentID;
	///报单引用
	char	*OrderRef = pTrade->OrderRef;
	///用户代码
	char	*UserID = pTrade->UserID;
	///交易所代码
	char	*ExchangeID =pTrade->ExchangeID;
	///成交编号
	char *	TradeID = pTrade->TradeID;
	
	///报单编号
	char	*OrderSysID = pTrade->OrderSysID;
	///会员代码
	//TThostFtdcParticipantIDType	ParticipantID;
	///客户代码
	char	*ClientID = pTrade->ClientID;
	///交易角色
	//TThostFtdcTradingRoleType	TradingRole;
	///合约在交易所的代码
	//TThostFtdcExchangeInstIDType	ExchangeInstID;
	
	///投机套保标志
	TThostFtdcHedgeFlagType	hedgeFlag = pTrade->HedgeFlag;
	char HedgeFlag[]={hedgeFlag,'\0'};
	//sprintf(HedgeFlag,"%s",hedgeFlag);
	///价格
	TThostFtdcPriceType	price = pTrade->Price;
	char Price[100];
	sprintf(Price,"%f",price);
	///数量
	TThostFtdcVolumeType	volume = pTrade->Volume;
	char Volume[100];
	sprintf(Volume,"%d",volume);
	///成交时期
	//TThostFtdcDateType	TradeDate;
	///成交时间
	char	*TradeTime = pTrade->TradeTime;
	///成交类型
	TThostFtdcTradeTypeType	tradeType = pTrade->TradeType;
	char TradeType[]={tradeType,'\0'};
	//sprintf(TradeType,"%s",tradeType);
	///成交价来源
	//TThostFtdcPriceSourceType	PriceSource;
	///交易所交易员代码
	//TThostFtdcTraderIDType	TraderID;
	///本地报单编号
	char	*OrderLocalID = pTrade->OrderLocalID;
	///结算会员编号
	//TThostFtdcParticipantIDType	ClearingPartID;
	///业务单元
	//TThostFtdcBusinessUnitType	BusinessUnit;
	///序号
	//TThostFtdcSequenceNoType	SequenceNo;
	///交易日
	char	*TradingDay = pTrade->TradingDay;
	///结算编号
	//TThostFtdcSettlementIDType	SettlementID;
	///经纪公司报单编号
	//TThostFtdcSequenceNoType	BrokerOrderSeq;
	tradeInfo.append("BrokerID=").append(BrokerID);tradeInfo.append("\t");
	tradeInfo.append("InvestorID=").append(InvestorID);tradeInfo.append("\t");
	tradeInfo.append("InstrumentID=").append(InstrumentID);tradeInfo.append("\t");
	tradeInfo.append("OrderRef=").append(OrderRef);tradeInfo.append("\t");
	tradeInfo.append("UserID=").append(UserID);tradeInfo.append("\t");
	tradeInfo.append("ExchangeID=").append(ExchangeID);tradeInfo.append("\t");
	tradeInfo.append("Direction=").append(Direction);tradeInfo.append("\t");
	tradeInfo.append("ClientID=").append(ClientID);tradeInfo.append("\t");
	tradeInfo.append("OffsetFlag=").append(OffsetFlag);tradeInfo.append("\t");
	tradeInfo.append("HedgeFlag=").append(HedgeFlag);tradeInfo.append("\t");
	tradeInfo.append("Price=").append(Price);tradeInfo.append("\t");
	tradeInfo.append("Volume=").append(Volume);tradeInfo.append("\t");
	tradeInfo.append("TradeTime=").append(TradeTime);tradeInfo.append("\t");
	tradeInfo.append("TradeType=").append(TradeType);tradeInfo.append("\t");
	tradeInfo.append("OrderLocalID=").append(OrderLocalID);tradeInfo.append("\t");
	tradeInfo.append("TradingDay=").append(TradingDay);tradeInfo.append("\t");
	tradeInfo.append("ordersysid=").append(OrderSysID);tradeInfo.append("\t");
	tradeInfo.append("tradeid=").append(TradeID);tradeInfo.append("\t");
	return tradeInfo;
}
int processtrade(CThostFtdcTradeField *pTrade)
{
	if(start_process == 0){
		return 0;
	}
	///买卖方向
	TThostFtdcDirectionType	direction = pTrade->Direction;
	char Direction[]={direction,'\0'};
	//sprintf(Direction,"%s",direction);
	///开平标志
	TThostFtdcOffsetFlagType	offsetFlag = pTrade->OffsetFlag;
	char OffsetFlag[]={offsetFlag,'\0'};
	///合约代码
	char	*InstrumentID =pTrade->InstrumentID;
	string str_inst = string(InstrumentID);
	//买卖方向
	string str_dir = string(Direction);
	//开平方向
	string str_offset = string(OffsetFlag);
    //锁持仓处理
    boost::recursive_mutex::scoped_lock SLock(pst_mtx);
    unordered_map<string,unordered_map<string,int>>::iterator map_iterator = positionmap.find(str_inst);
    //新开仓
    if(map_iterator == positionmap.end()){
        unordered_map<string,int> tmpmap;
        if(str_dir == "0"){//买
            //多头
            tmpmap["longTdPosition"] = pTrade->Volume;
            tmpmap["longYdPosition"] = 0;
            tmpmap["longTotalPosition"] = pTrade->Volume;
            //空头
            tmpmap["shortTdPosition"] = 0;
            tmpmap["shortYdPosition"] = 0;
            tmpmap["shortTotalPosition"] = 0;
        }else if(str_dir == "1"){//卖
            //空头
            tmpmap["shortTdPosition"] = pTrade->Volume;
            tmpmap["shortYdPosition"] = 0;
            tmpmap["shortTotalPosition"] = pTrade->Volume;
            //多头
            tmpmap["longTdPosition"] = 0;
            tmpmap["longYdPosition"] = 0;
            tmpmap["longTotalPosition"] = 0;
        }
        positionmap[str_inst] = tmpmap;
    }else{
        ///平仓
//        #define USTP_FTDC_OF_Close '1'
//        ///强平
//        #define USTP_FTDC_OF_ForceClose '2'
//        ///平今
//        #define USTP_FTDC_OF_CloseToday '3'
//        ///平昨
//        #define USTP_FTDC_OF_CloseYesterday '4'
        if(str_dir == "0"){//买
            if(str_offset == "0"){//买开仓,多头增加
                map_iterator->second["longTdPosition"] = map_iterator->second["longTdPosition"] + pTrade->Volume;
                int tmp_tdpst = map_iterator->second["longTdPosition"];
                int tmp_ydpst = map_iterator->second["longYdPosition"];
                realLongPstLimit = tmp_tdpst + tmp_ydpst;
                map_iterator->second["longTotalPosition"] = realLongPstLimit;
            }else if(str_offset == "1" ){//买平仓,空头减少
                int tmp_tdpst = map_iterator->second["shortTdPosition"];
                int tmp_ydpst = map_iterator->second["shortYdPosition"];
                //int tmp_num = map_iterator->second["shortTotalPosition"];
                if(tmp_tdpst > 0){
                    if(tmp_tdpst <= pTrade->Volume){
                        tmp_ydpst = tmp_ydpst - (pTrade->Volume - tmp_tdpst);
                        tmp_tdpst = 0;
                    }else{
                        tmp_tdpst = tmp_tdpst - pTrade->Volume;
                    }
                }else if(tmp_tdpst == 0){
                    tmp_ydpst = tmp_ydpst - pTrade->Volume;
                }else{
                    cout<<"tdposition is error!!!"<<endl;
                }
                realShortPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["shortTdPosition"] = tmp_tdpst;
                map_iterator->second["shortYdPosition"] = tmp_ydpst;
                map_iterator->second["shortTotalPosition"] = realShortPstLimit;
//                if(tmp_ydpst == 0){//buy open
//                    longPstIsClose = 1;
//                    long_offset_flag = 1;
//                }
            }else if(str_offset == "3"){//平今
                int tmp_tdpst = map_iterator->second["shortTdPosition"];
                int tmp_ydpst = map_iterator->second["shortYdPosition"];
                tmp_tdpst = tmp_tdpst - pTrade->Volume;
                realShortPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["shortTdPosition"] = tmp_tdpst;
                map_iterator->second["shortTotalPosition"] = realShortPstLimit;
            }else if(str_offset == "4"){//平昨
                int tmp_tdpst = map_iterator->second["shortTdPosition"];
                int tmp_ydpst = map_iterator->second["shortYdPosition"];
                if(tmp_ydpst == 0){
                    char c_err[100];
                    sprintf(c_err,"shortYdPosition is zero!!!,please check this rtn trade.");
                    cout<<c_err<<endl;
                    LOG(INFO)<<c_err;
                }
                tmp_ydpst = tmp_ydpst - pTrade->Volume;

                realShortPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["shortYdPosition"] = tmp_ydpst;
                map_iterator->second["shortTotalPosition"] = realShortPstLimit;
//                if(tmp_ydpst == 0){
//                    longPstIsClose = 1;
//                    long_offset_flag = 1;
//                }
            }
        }else if(str_dir == "1"){//卖
            if(str_offset == "0"){//卖开仓,空头增加
                map_iterator->second["shortTdPosition"] = map_iterator->second["shortTdPosition"] + pTrade->Volume;
                int tmp_tdpst = map_iterator->second["shortTdPosition"];
                int tmp_ydpst = map_iterator->second["shortYdPosition"];
                realShortPstLimit = tmp_tdpst + tmp_ydpst;
                map_iterator->second["shortTotalPosition"] = realShortPstLimit;
            }else if(str_offset == "1"){//卖平仓,多头减少
                int tmp_tdpst = map_iterator->second["longTdPosition"];
                int tmp_ydpst = map_iterator->second["longYdPosition"];
                //int tmp_num = map_iterator->second["longTotalPosition"];
                if(tmp_tdpst > 0){
                    if(tmp_tdpst <= pTrade->Volume){
                        tmp_ydpst = tmp_ydpst - (pTrade->Volume - tmp_tdpst);
                        tmp_tdpst = 0;
                    }else{
                        tmp_tdpst = tmp_tdpst - pTrade->Volume;
                    }
                }else if(tmp_tdpst == 0){
                    tmp_ydpst = tmp_ydpst - pTrade->Volume;
                }else{
                    cout<<"tdposition is error!!!"<<endl;
                }
                realLongPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["longTdPosition"] = tmp_tdpst;
                map_iterator->second["longYdPosition"] = tmp_ydpst;
                map_iterator->second["longTotalPosition"] = realLongPstLimit;
//                if(tmp_ydpst == 0){//sell open
//                    shortPstIsClose = 1;
//                    short_offset_flag = 1;
//                }
            }else if(str_offset == "3"){//平今
                int tmp_tdpst = map_iterator->second["longTdPosition"];
                int tmp_ydpst = map_iterator->second["longYdPosition"];
                tmp_tdpst = tmp_tdpst - pTrade->Volume;
                realLongPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["longTdPosition"] = tmp_tdpst;
                map_iterator->second["longTotalPosition"] = realLongPstLimit;
            }else if(str_offset == "4"){//平昨
                int tmp_tdpst = map_iterator->second["longTdPosition"];
                int tmp_ydpst = map_iterator->second["longYdPosition"];
                if(tmp_ydpst == 0){
                    char c_err[100];
                    sprintf(c_err,"longYdPosition is zero!!!,please check this rtn trade.");
                    cout<<c_err<<endl;
                    LOG(INFO)<<c_err;
                }
                tmp_ydpst = tmp_ydpst - pTrade->Volume;
                realLongPstLimit = tmp_ydpst + tmp_tdpst;
                map_iterator->second["longYdPosition"] = tmp_ydpst;
                map_iterator->second["longTotalPosition"] = realLongPstLimit;
//                if(tmp_ydpst == 0){//sell open
//                    shortPstIsClose = 1;
//                    short_offset_flag = 1;
//                }
            }
        }
    }
    tradeParaProcessTwo();
    string tmpmsg;
    for(unordered_map<string,unordered_map<string,int>>::iterator it=positionmap.begin();it != positionmap.end();it ++){
        tmpmsg.append(it->first).append("持仓情况:");
        char char_tmp_pst[10] = {'\0'};
        char char_longyd_pst[10] = {'\0'};
        char char_longtd_pst[10] = {'\0'};
        sprintf(char_tmp_pst,"%d",it->second["longTotalPosition"]);
        sprintf(char_longyd_pst,"%d",it->second["longYdPosition"]);
        sprintf(char_longtd_pst,"%d",it->second["longTdPosition"]);
        tmpmsg.append("多头数量=");
        tmpmsg.append(char_tmp_pst);
        tmpmsg.append(";今仓数量=");
        tmpmsg.append(char_longtd_pst);
        tmpmsg.append(";昨仓数量=");
        tmpmsg.append(char_longyd_pst);
        char char_tmp_pst2[10] = {'\0'};
        char char_shortyd_pst[10] = {'\0'};
        char char_shorttd_pst[10] = {'\0'};
        sprintf(char_tmp_pst2,"%d",it->second["shortTotalPosition"]);
        sprintf(char_shortyd_pst,"%d",it->second["shortYdPosition"]);
        sprintf(char_shorttd_pst,"%d",it->second["shortTdPosition"]);
        tmpmsg.append("空头数量=");
        tmpmsg.append(char_tmp_pst2);
        tmpmsg.append(";今仓数量=");
        tmpmsg.append(char_shorttd_pst);
        tmpmsg.append(";昨仓数量=");
        tmpmsg.append(char_shortyd_pst);
    }
    cout<<tmpmsg<<endl;
    LOG(INFO)<<tmpmsg;
	return 0;
}
void tradeParaProcessTwo(){
    for(unordered_map<string,unordered_map<string,int>>::iterator map_iterator=positionmap.begin();map_iterator != positionmap.end();map_iterator ++){
        string tmpmsg;
        realShortPstLimit = map_iterator->second["shortTotalPosition"];
        realLongPstLimit = map_iterator->second["longTotalPosition"];
        int shortYdPst = map_iterator->second["shortYdPosition"];
        int longYdPst = map_iterator->second["longYdPosition"];
        if(longYdPst > 0){
            shortPstIsClose = 2;
            short_offset_flag = 4;
        }
        if(shortYdPst > 0){
            longPstIsClose = 2;
            long_offset_flag = 4;
        }
        // buy or open judge
        if(realLongPstLimit > longpstlimit){ //多头超过持仓限额，且必须空头有持仓才能多头平仓
            char char_limit[10] = {'\0'};
            sprintf(char_limit,"%d",realLongPstLimit);
            longPstIsClose = 11;//long can not to open new position
            tmpmsg.append("多头持仓量=");
            tmpmsg.append(char_limit).append("大于longpstlimit,long can not to open new position");
        }else if(realShortPstLimit > shortpstlimit){//空头开平仓判断
            char char_limit[10] = {'\0'};
            sprintf(char_limit,"%d",realShortPstLimit);
            shortPstIsClose = 11;
            tmpmsg.append("空头持仓量=");
            tmpmsg.append(char_limit).append("大于shortpstlimit,short can not to open new position");
        }
        cout<<tmpmsg<<endl;
        LOG(INFO)<<tmpmsg;
        //spread set
        int bidAkdSpread = abs(realShortPstLimit - realLongPstLimit);
        if(bidAkdSpread >= firstGap && bidAkdSpread < secondGap && realShortPstLimit  > realLongPstLimit){
            bidCulTimes += 2;
            if(down_culculate >= bidCulTimes){
                down_culculate = (4*down_culculate)/5;
            }
        }else if(bidAkdSpread >= secondGap && realShortPstLimit > realLongPstLimit){
            bidCulTimes += 4;
            if(down_culculate >= bidCulTimes){
                down_culculate = (4*down_culculate)/5;
            }
        }else if(bidAkdSpread >= firstGap && bidAkdSpread < secondGap && realShortPstLimit < realLongPstLimit){
            askCulTimes += 2;
            if(up_culculate >= askCulTimes){
                up_culculate = (4*up_culculate)/5;
            }
        }else if(bidAkdSpread >= secondGap && realShortPstLimit < realLongPstLimit){
            askCulTimes += 4;
            if(up_culculate >= askCulTimes){
                up_culculate = (4*up_culculate)/5;
            }
        }else{
            bidCulTimes = cul_times;
            askCulTimes = cul_times;
        }
        lastABSSpread = bidAkdSpread;
    }
}
//将成交信息组装成对冲报单
CThostFtdcInputOrderField* assamble(CThostFtdcOrderField *pTrade)
{
    CThostFtdcInputOrderField* order = new CThostFtdcInputOrderField();
    memset(order,0,sizeof(order));
	//经济公司代码
    strcpy(order->BrokerID,pTrade->BrokerID);
	///投资者代码
    strcpy(order->InvestorID,pTrade->InvestorID);
	///合约代码
    strcpy(order->InstrumentID,pTrade->InstrumentID);
//	///报单引用
//	strcpy(order.OrderRef ,pTrade->OrderRef);
	///报单价格条件: 限价
    order->OrderPriceType = pTrade->OrderPriceType;
    ///买卖方向
//	TThostFtdcDirectionType	Direction = pTrade->Direction;
    order->Direction = pTrade->Direction;
	///组合开平标志: 和对手方一致
    strcpy(order->CombOffsetFlag,pTrade->CombOffsetFlag);
	///组合投机套保标志
    strcpy(order->CombHedgeFlag,pTrade->CombHedgeFlag);
	///价格
    order->LimitPrice = pTrade->LimitPrice;
    ///数量:
    //order->VolumeTotalOriginal = pTrade->VolumeTotalOriginal;
	///有效期类型: 当日有效
    order->TimeCondition = pTrade->TimeCondition;
	///成交量类型: 任何数量
    order->VolumeCondition = pTrade->VolumeCondition;
	///最小成交量: 1
    order->MinVolume = pTrade->MinVolume;
	///触发条件: 立即
    order->ContingentCondition = pTrade->ContingentCondition;
	///强平原因: 非强平
    order->ForceCloseReason = pTrade->ForceCloseReason;
	///自动挂起标志: 否
    order->IsAutoSuspend = pTrade->IsAutoSuspend;
	///用户强评标志: 否
    order->UserForceClose = pTrade->UserForceClose;
    return order;

}

