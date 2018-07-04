#include <sstream>
#include <string>
#include <algorithm>
#include <time.h>
#include <thread>
#include <direct.h>
#include <io.h>
#include <windows.h>

#include "config.h"
#include "CtpAdaptor.h"

////////////////////////////////////////////////////////////////////////////////
void SystemTimeToTimeval(
    const SYSTEMTIME& systime, struct timeval* t)
{
    FILETIME filetime;
    SystemTimeToFileTime(&systime, &filetime);
    __int64 low = filetime.dwLowDateTime;
    __int64 high = filetime.dwHighDateTime;

    // SystemTime is in 100-nanos. So this is 1/10th of a microsecond.
    const __int64 kMicrosPerSecond = 1000000;
    const __int64 kNanos100PerMicro = 10;
    const __int64 kWindowsEpochDifference = 11644473600LL;  // 1/1/1970 - 1/1/1601
    __int64 nanos100SinceWindowsEpoch = (high << 32 | low);
    __int64 microsSinceWindowsEpoch = nanos100SinceWindowsEpoch / kNanos100PerMicro;
    __int64 secsSinceWindowsEpoch = microsSinceWindowsEpoch / kMicrosPerSecond;
    __int64 secs = secsSinceWindowsEpoch - kWindowsEpochDifference;
    t->tv_sec = secs;
    t->tv_usec = microsSinceWindowsEpoch % kMicrosPerSecond;
}

// tm is local time
time_t timegm(struct tm* tm)
{
    SYSTEMTIME systime;
    systime.wYear = tm->tm_year + 1900;
    systime.wMonth = tm->tm_mon + 1;
    systime.wDayOfWeek = tm->tm_wday;
    systime.wDay = tm->tm_mday;
    systime.wHour = tm->tm_hour;
    systime.wMinute = tm->tm_min;
    systime.wSecond = tm->tm_sec;
    systime.wMilliseconds = 0;

    struct timeval t;
    SystemTimeToTimeval(systime, &t);
    return t.tv_sec;
}

inline struct timeval make_timeval(time_t sec, int usec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = usec;
    return tv;
}

static void split(const std::string& s, const std::string& delim, std::vector<std::string>& result)
{
    size_t last = 0;
    size_t index = s.find_first_of(delim, last);
    while (index != std::string::npos) {
        result.push_back(s.substr(last, index - last));
        last = index + 1;
        index = s.find_first_of(delim, last);
    }

    if (index - last > 0) {
        result.push_back(s.substr(last, index - last));
    }
}

////////////////////////////////////////////////////////////////////////////////
class MarketDataSpi : public CThostFtdcMdSpi
{
public:
    MarketDataSpi(CtpInputAdaptor *adaptor, CThostFtdcMdApi *api)
    {
        m_pAdaptor = adaptor;
        m_pMktDataApi = api;
    }

    virtual void OnFrontConnected()
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnMdFrontConnected();
        }
    }

    virtual void OnFrontDisconnected(int nReason)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnMdFrontDisconnected(nReason);
        }
    }

    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnMdRspUserLogin(pRspUserLogin, pRspInfo, nRequestID, bIsLast);
        }
    }

    virtual void OnRtnDepthMarketData(CThostFtdcDepthMarketDataField *pMarketData)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnMdRtnDepthMarketData(pMarketData);
        }
    }

    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnMdRspError(pRspInfo, nRequestID, bIsLast);
        }
    }

private:
    CtpInputAdaptor *m_pAdaptor;
    CThostFtdcMdApi* m_pMktDataApi;
};

class TraderSpi : public CThostFtdcTraderSpi
{
public:
    TraderSpi(CtpInputAdaptor *adaptor, CThostFtdcTraderApi *api)
    {
        m_pAdaptor = adaptor;
        m_pTraderApi = api;
    }

    virtual void OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnTradeRspError(pRspInfo, nRequestID, bIsLast);
        }
    }

    virtual void OnFrontConnected()
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnTradeFrontConnected();
        }
    }

    virtual void OnFrontDisconnected(int nReason)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnTradeFrontDisconnected(nReason);
        }
    }

    virtual void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnTradeRspUserLogin(pRspUserLogin, pRspInfo, nRequestID, bIsLast);
        }
    }

    virtual void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
    {
        if (m_pAdaptor) {
            m_pAdaptor->OnRspQryInstrument(pInstrument, pRspInfo, nRequestID, bIsLast);
        }
    }

private:
    CtpInputAdaptor *m_pAdaptor;
    CThostFtdcTraderApi *m_pTraderApi;
};

////////////////////////////////////////////////////////////////////////////////
CtpInputAdaptor::CtpInputAdaptor()
    : m_configFile("MarketHub.ini")
{
    m_pMdApi = nullptr;
    m_pTraderApi = nullptr;
    m_pMdSpi = nullptr;
    m_pTraderSpi = nullptr;
    m_requestId = 0;
}

CtpInputAdaptor::~CtpInputAdaptor()
{
}

int CtpInputAdaptor::type() const
{
    return SOURCE;
}

const char* CtpInputAdaptor::getName() const
{
    return CTP_ADAPTOR_NAME;
}

bool CtpInputAdaptor::initialize()
{
    // make flow path.
    if (_access(DEFAULT_FLOW_PATH, 0) != 0) {
        _mkdir(DEFAULT_FLOW_PATH);
    }

    if (m_pMdApi == nullptr) {
        m_pMdApi = CThostFtdcMdApi::CreateFtdcMdApi(DEFAULT_FLOW_PATH);
        if (m_pMdApi == nullptr) {
            return false;
        }
    }

    if (m_pTraderApi == nullptr) {
        m_pTraderApi = CThostFtdcTraderApi::CreateFtdcTraderApi(DEFAULT_FLOW_PATH);
        if (m_pTraderApi == nullptr) {
            m_pMdApi->Release();
            m_pMdApi = nullptr;
            return false;
        }
    }

    if (m_pMdSpi == nullptr) {
        m_pMdSpi = new MarketDataSpi(this, m_pMdApi);
        m_pMdApi->RegisterSpi(m_pMdSpi);
    }

    if (m_pTraderSpi == nullptr) {
        m_pTraderSpi = new TraderSpi(this, m_pTraderApi);
        m_pTraderApi->RegisterSpi(m_pTraderSpi);
        m_pTraderApi->SubscribePrivateTopic(THOST_TERT_RESUME);
        m_pTraderApi->SubscribePublicTopic(THOST_TERT_RESUME);
    }

    m_loginTimeInit = false;

    sendLogMessage(LOG_INFO, "%s: Initialized.", getName());

    notifyStatusChanged(ADAPTOR_INIT, nullptr);

    return true;
}

bool CtpInputAdaptor::onStart()
{
    if (!initialize()) {
        return false;
    }

    if (!loadConfig()) {
        return false;
    }

    if (!login()) {
        return false;
    }

    return true;
}

bool CtpInputAdaptor::onStop()
{
    if (m_pMdApi != nullptr) {
        m_pMdApi->RegisterSpi(nullptr);
        m_pMdApi->Release();
        m_pMdApi = nullptr;
    }

    if (m_pMdSpi != nullptr) {
        delete m_pMdSpi;
        m_pMdSpi = nullptr;
    }

    if (m_pTraderApi != nullptr) {
        m_pTraderApi->RegisterSpi(nullptr);
        m_pTraderApi->Release();
        m_pTraderApi = nullptr;
    }

    if (m_pTraderSpi != nullptr) {
        delete m_pTraderSpi;
        m_pTraderSpi = nullptr;
    }

    return true;
}

void CtpInputAdaptor::OnMdFrontConnected()
{
    sendLogMessage(LOG_INFO, "%s: Market data front connected!", getName());

    CThostFtdcReqUserLoginField f = { 0 };
    m_pMdApi->ReqUserLogin(&f, 0);
}

void CtpInputAdaptor::OnMdFrontDisconnected(int nReason)
{
    m_subscribed = false;

    sendLogMessage(LOG_ERROR, "%s: Market data front disconnected!", getName());
}

void CtpInputAdaptor::OnMdRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID == 0) {
        sendLogMessage(LOG_INFO, "%s: Market data front login succeed!", getName());
        m_bMdUserLogin = true;

        if (!m_subscribed && m_allInstrumentInfoFetched) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            subscribeMarketData();
            m_subscribed = true;
        }
    }
}

void CtpInputAdaptor::OnMdRtnDepthMarketData(CThostFtdcDepthMarketDataField *pMarketData)
{
    if (pMarketData != nullptr) {
        sendDepthMarketData(pMarketData);
    }
}

void CtpInputAdaptor::OnMdRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo != nullptr && pRspInfo->ErrorMsg[0] != '\0') {
        sendLogMessage(LOG_ERROR, pRspInfo->ErrorMsg);
    }
}

void CtpInputAdaptor::OnTradeRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo != nullptr && pRspInfo->ErrorMsg[0] != '\0') {
        sendLogMessage(LOG_ERROR, pRspInfo->ErrorMsg);
    }
}

void CtpInputAdaptor::OnTradeFrontConnected()
{
    CThostFtdcReqUserLoginField f = { 0 };
    strcpy_s(f.BrokerID, m_brokerId.c_str());
    strcpy_s(f.UserID, m_username.c_str());
    char plaintext[128] = { 0 };
    if (Controller::decrypt(m_passwd.c_str(), plaintext)) {
        strcpy_s(f.Password, plaintext);
    }
    m_pTraderApi->ReqUserLogin(&f, ++m_requestId);
}

void CtpInputAdaptor::OnTradeFrontDisconnected(int nReason)
{
    sendLogMessage(LOG_ERROR, "%s: Trade front disconnected, reason:%d.", getName(), nReason);
}

void CtpInputAdaptor::OnTradeRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo->ErrorID == 0) {

        initLocalTimestamp(pRspUserLogin->LoginTime);

        m_allInstrumentInfoFetched = false;
        m_instruments.clear();
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        CThostFtdcQryInstrumentField QryInstrument = { 0 };
        m_pTraderApi->ReqQryInstrument(&QryInstrument, ++m_requestId);
    } else {
        sendLogMessage(LOG_ERROR, pRspInfo->ErrorMsg);
    }
}

void CtpInputAdaptor::OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pInstrument != nullptr && (pRspInfo == nullptr || pRspInfo->ErrorID == 0)) {
        if (!filterInstrument(pInstrument->InstrumentID)) {
            m_instruments.insert(std::make_pair(pInstrument->InstrumentID, pInstrument->ExchangeID));
        }
    }

    if (bIsLast) {
        int size = m_instruments.size();
        if (m_bMdUserLogin && size > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            subscribeMarketData();
            m_subscribed = true;
        }

        m_allInstrumentInfoFetched = true;

        notifyStatusChanged(ADAPTOR_RUNNING, nullptr);
    }
}

bool CtpInputAdaptor::loadConfig()
{
    rude::Config config;
    if (config.load(m_configFile.c_str())) {
        config.setSection("CTP");
        m_brokerId = config.getStringValue("BrokerID");
        m_mdFront = config.getStringValue("MdFront");
        m_tradeFront = config.getStringValue("TradeFront");
        m_username = config.getStringValue("UserName");
        m_passwd = config.getStringValue("UserPwd");
		m_subscribeAllQuotes = config.getBoolValue("SubscribeAllQuotes");

        config.setSection("CacheData");
        int num = config.getNumDataMembers();
        if (num > 0) {
            for (int i = 0; i < num; i++) {
                const char *data = config.getDataNameAt(i);
                if (data != nullptr) {
					char line[256];
					strcpy(line, data);
					char* p = strchr(line, ',');
					if (p) {
						*p = '\0';
						m_subscriptions.push_back(line);
					}

                }
            }
        }

        config.setSection("Filter");
        num = config.getNumDataMembers();
        for (int i = 0; i < num; i++) {
            const char *data = config.getDataNameAt(i);
            if (data != nullptr) {
                m_symbolFilters.push_back(data);
            }
        }

        return true;
    } else {
        sendLogMessage(LOG_INFO, "%s: Could not load config file '%s'.", getName(), m_configFile.c_str());
    }

    return false;
}

bool CtpInputAdaptor::login()
{
    if (m_pMdApi) {
        std::vector<std::string> fonts;
        split(m_mdFront, ";", fonts);
        for (unsigned int i = 0; i < fonts.size(); i++) {
            if (fonts[i].length() > 0) {
                char font[256];
                _snprintf_s(font, sizeof(font), "tcp://%s", fonts[i].c_str());
                font[sizeof(font)-1] = '\0';
                m_pMdApi->RegisterFront(font);
            }
        }
        m_pMdApi->Init();
    }

    m_loginTimeInit = false;

    if (m_pTraderApi) {
        std::vector<std::string> fonts;
        split(m_tradeFront, ";", fonts);
        for (unsigned int i = 0; i < fonts.size(); i++) {
            if (fonts[i].length() > 0) {
                char font[256];
                _snprintf_s(font, sizeof(font), "tcp://%s", fonts[i].c_str());
                font[sizeof(font)-1] = '\0';
                m_pTraderApi->RegisterFront(font);
            }
        }
        m_pTraderApi->Init();
    }

    return true;
}

bool CtpInputAdaptor::subscribeMarketData()
{
    bool ret = false;

    vector<string> instrList;

	if (m_subscribeAllQuotes) {
		for (auto& instr : m_instruments) {
			instrList.push_back(instr.first);
		}
	} else {
		for (auto& instr : m_subscriptions) {
			instrList.push_back(instr);
		}
	}

    std::sort(instrList.begin(), instrList.end());

    int size = instrList.size();

    char buf[128];
    sprintf_s(buf, "%s: Subscribe instruments number: (%d)", getName(), size);
    sendLogMessage(LOG_INFO, buf);
//    sendLogMessage("================================");
    char **ppInstrumentID = new char*[size];
    int idx = 0;

    for (auto itor = instrList.begin(); itor != instrList.end(); itor++) {
        ppInstrumentID[idx] = new char[32];
        strncpy_s(ppInstrumentID[idx], 32, itor->c_str(), 32);
//        sendLogMessage(ppInstrumentID[idx]);
        idx++;
    }

//    sendLogMessage("================================");


    ret = (m_pMdApi->SubscribeMarketData(ppInstrumentID, size) != 0);

    for (int i = 0; i < idx; i++) {
        delete[] ppInstrumentID[i];
    }
    delete[] ppInstrumentID;

    return ret;
}

bool CtpInputAdaptor::filterInstrument(const char* instrument)
{
    if (instrument == NULL) {
        return true;
    }

    for (size_t i = 0; i < m_symbolFilters.size(); i++) {
        const char* p = m_symbolFilters[i].c_str();
        const char* s = instrument;
        int len = strlen(p);
        int idx = 0;

        while (idx < len) {
            if (s[idx] != '\0' && p[idx] != '\0' && s[idx] == p[idx]) {
                idx++;
                continue;
            } else {
                break;
            }
        }
        if (idx == len) {
            if (s[idx] == ' ') {
                return true;
            }
        }
    }

    return false;
}

// Example:
// datestr: 20140828
// timestr: 09:47:04
long long CtpInputAdaptor::cvtDateTimeStr(const char datestr[9], const char timestr[9])
{
    const char *p = datestr;
    char year[5] = { 0 };
    memcpy(year, p, 4);
    p += 4;

    char month[3] = { 0 };
    memcpy(month, p, 2);
    p += 2;

    char day[3] = { 0 };
    memcpy(day, p, 2);

    p = timestr;
    char hour[3] = { 0 };
    memcpy(hour, p, 2);
    p += 3;

    char min[3] = { 0 };
    memcpy(min, p, 2);
    p += 3;

    char sec[3] = { 0 };
    memcpy(sec, p, 2);

    struct tm utc;
    memset(&utc, 0, sizeof(utc));
    utc.tm_year = atoi(year);
    utc.tm_mon = atoi(month);
    utc.tm_mday = atoi(day);
    utc.tm_hour = atoi(hour);
    utc.tm_min = atoi(min);
    utc.tm_sec = atoi(sec);

    utc.tm_year -= 1900;
    --utc.tm_mon;

    struct timeval t_ = make_timeval(timegm(&utc), 0);

    return t_.tv_sec;
}

// Example:
// timestr: 23:12:43
unsigned long CtpInputAdaptor::cvtTimeStr(const char timestr[9])
{
    if (timestr[0] == '\0') {
        return SECS_ONE_DAY;
    }

    const char *p = timestr;
    char hour[3] = { 0 };
    memcpy(hour, p, 2);
    p += 3;

    char min[3] = { 0 };
    memcpy(min, p, 2);
    p += 3;

    char sec[3] = { 0 };
    memcpy(sec, p, 2);

    unsigned long secs = atoi(hour) * 3600 + atoi(min) * 60 + atoi(sec);

    return secs;
}

void CtpInputAdaptor::initLocalTimestamp(const char loginTime[9])
{
    unsigned long secs = cvtTimeStr(loginTime);
    m_loginLocalTickCount = GetTickCount();
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    m_localTradingDay = lt.wYear * 10000 + lt.wMonth * 100 + lt.wDay;

    if (secs >= SECS_ONE_DAY) {
        m_loginTimeInit = false;
    } else {
        m_loginLocalSecs = m_loginLocalTickCount / 1000;
        m_loginTimeSecs = secs;
        m_loginTimeInit = true;
    }
}

bool CtpInputAdaptor::checkTimestamp(const char time[9])
{
    // if login time is not initialized, we assume all timestamp is OK.
    if (!m_loginTimeInit) {
        return true;
    }

    int secs = cvtTimeStr(time);
    if (secs >= SECS_ONE_DAY) {
        return false;
    }

    DWORD tick = GetTickCount();
    unsigned long localElapsedSecs = (TICKS_DIFF(m_loginLocalTickCount, tick));
    localElapsedSecs /= 1000;
    unsigned long mdElapseSecs = SECS_DIFF(m_loginTimeSecs, secs);

    // 如果行情时间落后于登陆时间一小段，认为行情是有效的
    int elapse = SECS_ONE_DAY - (int)mdElapseSecs;
    if (abs(elapse) < MARKETDATA_TIME_OFFSET_THRESHOLD) {
        return true;
    }

    unsigned long offset = 0;
    if (localElapsedSecs > mdElapseSecs) {
        offset = localElapsedSecs - mdElapseSecs;
    } else {
        offset = mdElapseSecs - localElapsedSecs;
    }

    if (offset >= MARKETDATA_TIME_OFFSET_THRESHOLD) {
        return false;
    } else {
        return true;
    }
}

// Example:
// date string: 20140828
// time string: 09:47:04
void CtpInputAdaptor::convertDateTimeStr(const char dateStr[9], const char timeStr[9], int* date, int* time)
{
    const char *p = dateStr;
    char year[5] = { 0 };
    memcpy(year, p, 4);
    p += 4;

    char month[3] = { 0 };
    memcpy(month, p, 2);
    p += 2;

    char day[3] = { 0 };
    memcpy(day, p, 2);

    p = timeStr;
    char hour[3] = { 0 };
    memcpy(hour, p, 2);
    p += 3;

    char min[3] = { 0 };
    memcpy(min, p, 2);
    p += 3;

    char sec[3] = { 0 };
    memcpy(sec, p, 2);

    *date = atoi(year) * 10000 + atoi(month) * 100 + atoi(day);
    *time = atoi(hour) * 10000 + atoi(min) * 100 + atoi(sec);

}

bool CtpInputAdaptor::sendDepthMarketData(CThostFtdcDepthMarketDataField *pMarketData)
{
    if (!checkTimestamp(pMarketData->UpdateTime)) {
        return false;
    }

    FutureMarketData data = { 0 };
    convertDateTimeStr(pMarketData->TradingDay, pMarketData->UpdateTime, &(data.tradingDay), &(data.updateTime));
    data.updateMillisec = pMarketData->UpdateMillisec;
    memcpy(data.instrument, pMarketData->InstrumentID, sizeof(data.instrument));

    int time = data.updateTime;
    int h = time / 10000;
    int m = (time % 10000) / 100;
    int s = time % 100;
    int sec_count = SECOND_COUNT(h, m, s);
    if (data.tradingDay > m_localTradingDay) {
        if (sec_count >= SECOND_COUNT(21, 0, 0) && sec_count < SECOND_COUNT(24, 0, 0)) {
            data.tradingDay = m_localTradingDay;
        }
    }

    if (pMarketData->ExchangeID[0] != '\0') {
        memcpy(data.exchange, pMarketData->ExchangeID, sizeof(data.exchange));
    } else {
        auto it = m_instruments.find(pMarketData->InstrumentID);
        if (it != m_instruments.end()) {
            strcpy(data.exchange, it->second.c_str());
        }
    }

    data.lastPrice = pMarketData->LastPrice;
    data.openPrice = pMarketData->OpenPrice;
    data.highestPrice = pMarketData->HighestPrice;
    data.lowestPrice = pMarketData->LowestPrice;
    data.volume = pMarketData->Volume;
    data.turnover = pMarketData->Turnover;
    data.openInterest = pMarketData->OpenInterest;
    data.closePrice = pMarketData->ClosePrice;
    data.upperLimitPrice = pMarketData->UpperLimitPrice;
    data.lowerLimitPrice = pMarketData->LowerLimitPrice;
    data.bidPrice1 = pMarketData->BidPrice1;
    data.bidVolume1 = pMarketData->BidVolume1;
    data.askPrice1 = pMarketData->AskPrice1;
    data.askVolume1 = pMarketData->AskVolume1;

    return notifyMarketData(MsgType_FutureMarketData, &data);
}

EXPORT_ADAPTOR(CtpInputAdaptor)