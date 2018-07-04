#ifndef CTP_ADAPTOR_H
#define CTP_ADAPTOR_H

#include <vector>
#include <string>
#include <unordered_map>

#include "ThostFtdcMdApi.h"
#include "ThostFtdcTraderApi.h"

#include "MarketHub.h"
#include "Adaptor.h"

using namespace MarketHub;

using std::vector;
using std::string;
using std::unordered_map;

#define CTP_ADAPTOR_NAME   "CTP"

#define DEFAULT_FLOW_PATH          "./CtpFlowFiles/"

#define SECOND_COUNT(h, m, s)               ((h) * 3600 + (h) * 60 + (s))
#define SECS_ONE_DAY                        (24 * 3600)
#define MARKETDATA_TIME_OFFSET_THRESHOLD    (5 * 60)

#define TICKS_DIFF(prev, cur) ((cur) >= (prev)) ? ((cur)-(prev)) : ((0xFFFFFFFF-(prev))+(cur))
#define SECS_DIFF(prev, cur)  ((cur) >= (prev)) ? ((cur)-(prev)) : ((SECS_ONE_DAY-(prev))+(cur))

class CtpInputAdaptor : public Adaptor
{
    friend class MarketDataSpi;
    friend class TraderSpi;

public:
    CtpInputAdaptor();
    ~CtpInputAdaptor();
    int type() const;
    const char* getName() const;
    bool onStart();
    bool onStop();

private:
    void OnMdFrontConnected();
    void OnMdFrontDisconnected(int nReason);
    void OnMdRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnMdRtnDepthMarketData(CThostFtdcDepthMarketDataField *pMarketData);
    void OnMdRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnTradeRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnTradeFrontConnected();
    void OnTradeFrontDisconnected(int nReason);
    void OnTradeRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    bool initialize();
    bool loadConfig();
    bool login();
    bool subscribeMarketData();
    bool filterInstrument(const char* instrument);
    long long cvtDateTimeStr(const char daystr[9], const char timestr[9]);
    unsigned long cvtTimeStr(const char timestr[9]);
    void initLocalTimestamp(const char loginTime[9]);
    bool checkTimestamp(const char time[9]);
    void convertDateTimeStr(const char dateStr[9], const char timeStr[9], int* date, int* time);
    bool sendDepthMarketData(CThostFtdcDepthMarketDataField *pMarketData);

private:
    CThostFtdcMdApi *m_pMdApi;
    CThostFtdcMdSpi *m_pMdSpi;

    CThostFtdcTraderApi *m_pTraderApi;
    CThostFtdcTraderSpi *m_pTraderSpi;

    volatile unsigned long m_requestId;
    const string m_configFile;
    std::unordered_map<string, string> m_instruments;
    string m_brokerId;
    string m_mdFront;
    string m_tradeFront;
    string m_username;
    string m_passwd;
	bool m_subscribeAllQuotes;

    bool m_bMdUserLogin;
    bool m_allInstrumentInfoFetched;
    bool m_subscribed;
	vector<string> m_subscriptions;

    vector<string> m_symbolFilters;

    bool m_loginTimeInit;
    unsigned long m_loginTimeSecs; // in seconds
    unsigned long m_loginLocalSecs;
    unsigned long m_loginLocalTickCount;
    unsigned long m_localTradingDay;
};

#endif // CTP_ADAPTOR_H