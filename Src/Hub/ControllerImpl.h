#ifndef CONTROLLER_IMPL_H
#define CONTROLLER_IMPL_H

#include <unordered_map>
#include <vector>
#include "EventHub.h"
#include "Adaptor.h"
#include "Server.h"
#include "Logger.h"
#include "Composer.h"
#include "Cache_Redis.h"
#include "Cache_Local.h"
#include "DataSource_MySql.h"
#include "DataSource_Csv.h"

namespace MarketHub
{
    
#define ENCRYPT_PASSWORD  (1)
#define PWD_ENCRYPT_KEY   ("pwd_encrypt_key")

class ControllerImpl : public EventListener, Subscriber
{
public:
    ControllerImpl();
    ~ControllerImpl(); 
    bool initialize();
    int  loadAdaptor(const char* library);
    void run();
	bool getKey(char* key, const char* instrument, int interval, int resolution);
	void subscribeMsg(int msgType, Subscriber* subscriber);
	void sendAdaptorStatus(int id, int type, const char* name, int status, const char* extra);
	void sendMarketData(int adaptorId, int type, void* data);    
	bool sendLogMsg(int level, const char* msgFmt, ...);
	int  retrieveData(const char*instrument, int resolution, int interval, int num, vector<string>& rows);
    void stop();
	void onMessage(const Message* msg);

    static bool decrypt(const char* enc, const char* key, char* plaintext);

private:
	bool sendMessage(const Message& msg);

    bool onEvent(const void* evt);
    bool dispatchEvent(const Message* pMsg);
    unsigned long getNextRuntimeId();

	bool installComposer(const char* instrument, int resolution, int interval, TradingHours& hours);
	void parseConfig(const char* file);
	string getSymbol(const char* instrument);
	void splitHours(char* input, TradingHours& hours);
	void splitContracts(char* input, vector<string>& contracts);
	void parseTradingHours(const char* period);
	void parsePreloadConfigLine(const char* configLine);
	void preloadHistData();
	void startCaching();
	const char* convertStrNum(char *dest, double num, int reserved = 0);
    bool printBarData(char* out, const FutureMarketData& data);
    bool printTickData(char* out, const FutureMarketData& data);
	bool printMarketData(char* out, int resolution, const FutureMarketData& data);
    void cacheData(const FutureMarketData* data);
	void doComposition(const FutureMarketData* data);

private:
    std::atomic_uint_least32_t m_nextRtId;

    Logger* m_logger;

    std::vector<Runtime*> m_runtimes;

    EventHub m_evtHub;

    bool m_srvEnable;
	Server m_server;

	std::unordered_map<int, std::vector<Subscriber*> > m_subscribers;

	typedef struct _ComposerContex {
		char instrument[32];
		int interval;
		int resolution;
		Composer* composer;
		bool hasUncompletedRow;
		char key[64];
		_ComposerContex()
		{
			instrument[0] = '\0';
			interval = 0;
			resolution = 0;
			memset(key, 0, sizeof(key));
			composer = nullptr;
			hasUncompletedRow = false;
		}
	} ComposerContex;

	std::unordered_map<std::string, std::vector<ComposerContex> > m_composers;
	std::unordered_map<string, TradingHours> m_tradingHours;

	bool m_subscribeAllMarketData;
	std::unordered_map<std::string, int> m_subscriptions;

    enum {
        DataSrc_Unknown = 0,
        DataSrc_MYSQL,
        DataSrc_CSV,
        DataSrc_WIND,
    };

	typedef struct {
		char instrument[64];
		int  resolution;
		int  interval;
		int  number;
        int  source;
		TradingHours tradingHours;
	} PreloadDataConfig;

	string m_configFileName;

	vector<PreloadDataConfig> m_preloadDataConfig;

    std::unordered_map<std::string, int> m_instrumentDS;

//	Cache_Redis m_cacheRedis;
	Cache_Local m_cacheLocal;
	DataSource_MySql m_dsMySql;
    DataSource_Csv m_dsCsv;

	bool m_isRunning;
};

} // namespace MarketHub

#endif // CONTROLLER_IMPL_H