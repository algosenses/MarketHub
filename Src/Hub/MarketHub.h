#ifndef MARKET_HUB_H
#define MARKET_HUB_H

#include <vector>
#include "Export.h"

namespace MarketHub
{

typedef struct _DateTime {
	int date;
	int time;
	int ms;
	_DateTime() {
		date = 0;
		time = 0;
		ms = 0;
	}
} DateTime;

#define INSTRUMENT_NAME_MAX_LEN   (32)

enum {
    LOG_TRACE,
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
};

typedef struct {
    char      instrument[32];
    char      exchange[8];
    int       tradingDay;  // YYYYMMDD
    int       updateTime;  // hhmmss
    int       updateMillisec;
    double    openPrice;
    double    lastPrice;
    double    closePrice;
    double    highestPrice;
    double    lowestPrice;
    double    upperLimitPrice;
    double    lowerLimitPrice;
    long long openInterest;
    long long volume;
    double    turnover;
    double    preSettlePrice;
    double    preClosePrice;
    double    settlePrice;
    int       preOpenInterest;
    double    averagePrice;

    double    bidPrice1;
    int       bidVolume1;
#if 0
    double    bidPrice2;
    int       bidVolume2;
    double    bidPrice3;
    int       bidVolume3;
    double    bidPrice4;
    int       bidVolume4;
    double    bidPrice5;
    int       bidVolume5;
#endif

    double    askPrice1;
    int       askVolume1;
#if 0
    double    askPrice2;
    int       askVolume2;
    double    askPrice3;
    int       askVolume3;
    double    askPrice4;
    int       askVolume4;
    double    askPrice5;
    int       askVolume5;
#endif

    bool      completed;
} FutureMarketData;

typedef struct {

} StockMarketData;

typedef struct {

} OptionMarketData;

typedef struct {
	int date;
	int time;
	int ms;
	int level;
    char text[256];
} Log;

enum {
    ADAPTOR_LOAD,
    ADAPTOR_INIT,
    ADAPTOR_START,
    ADAPTOR_RUNNING,
    ADAPTOR_PAUSE,
    ADAPTOR_RESUME,
    ADAPTOR_STOP
};

enum {
    ADAPTOR_INPUT,
    ADAPTOR_OUTPUT
};

typedef struct {
    int cmd;
    int param;
} AdaptorCommand;

typedef struct {
    int  id;
    int  type;
    char name[32];
    int  status;
    char extraData[256];
} AdaptorStatus;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int min;
    int sec;
    int millisec;
} TimeSync;

enum {
    MsgType_Invalid = 0,
    MsgType_Log,
    MsgType_AdaptorCommand,
    MsgType_AdaptorStatus,
    MsgType_FutureMarketData,
    MsgType_StockMarketData,
    MsgType_OptionMarketData,
    MsgType_TimeSync,
    MsgType_Command,
    MsgType_End
};

#define MSG_MAX_SIZE    (512)

typedef struct {
    int type;
    int source;
    int target;
} MsgHeader;

typedef union {
    Log                   log;
    TimeSync              timeSync;
    AdaptorCommand        adaptorCmd;
    AdaptorStatus         adaptorStatus;
    FutureMarketData      futureData;
    StockMarketData       stockData;
} MsgBody;

#pragma pack(push)
#pragma pack(1)
typedef struct {
    MsgHeader header;
    MsgBody   body;
    char    padding[MSG_MAX_SIZE - sizeof(MsgHeader)-sizeof(MsgBody)];
} Message;
#pragma pack(pop)

static_assert(sizeof(Message) == MSG_MAX_SIZE, "Message size is not equal to 512.");

class ControllerImpl;

class DllExport Subscriber
{
public:
	virtual ~Subscriber() {}
	virtual void onMessage(const Message* msg) = 0;
};

class DllExport Controller
{
public:
    static Controller* getInstance();
    
    void subscribeMsg(int msgType, Subscriber* subscriber);
    bool initialize();
    int  loadAdaptor(const char* library);
    void run();
    void stop();
    void release();

    static bool decrypt(const char* enc, char* plaintext);

private:
    Controller();
    Controller(const Controller&) = delete;
    Controller& operator=(const Controller&) = delete;
    ~Controller();

private:
    static Controller* m_instance;

    ControllerImpl* m_implementor;
};

class DataSource
{
public:
    virtual ~DataSource() {}
    virtual bool onStart() { return true; }
    // Latest data at the front(rows[0]).
    virtual int  retrieveData(const char*instrument, 
                              int resolution, 
                              int interval, int num, 
                              const DateTime& end, 
                              std::vector<FutureMarketData>& rows) = 0;
    virtual bool onStop() { return true; }
};

class DataCache 
{
public:
    virtual ~DataCache() {}
    virtual bool onStart() { return true; }
    virtual void storeData(const char*instrument, 
                           int resolution, 
                           int interval, 
                           const std::vector<FutureMarketData>& data) = 0;
    // Latest data at the front(rows[0])
    virtual int retrieveData(const char*instrument, 
                             int resolution, 
                             int interval, 
                             int num, 
                             std::vector<FutureMarketData>& rows) = 0;
    virtual bool appendNewBarRow(const char* key, const FutureMarketData& row) { return false; }
    virtual bool updateLastBarRow(const char* key, const FutureMarketData& row) { return false; }
    virtual bool appendNewTickRow(const char*instrument, const FutureMarketData& row) { return false; }
    virtual bool onStop() { return true; }
};

} // namespace MarketHub

#endif // MARKET_HUB_H