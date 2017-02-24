class TradeProcess{
public:
	void startTrade();
	void datainit();//初始化数据
	void tradeinit();//启动交易进程
	void initThread(int sendtype);
};