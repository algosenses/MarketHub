#ifndef CACHE_REDIS_H
#define CACHE_REDIS_H

#include <unordered_map>
#include <string>
#include <mutex>
#include <hiredis.h>

#include "MarketHub.h"

namespace MarketHub
{

class Cache_Redis
{
public:
    Cache_Redis(ControllerImpl* controller);
	bool onStart();
	void storeData(const char* key, vector<FutureMarketData>& data);
	int  retrieveData(const char* key, int num, vector<string>& rows);
	bool appendNewRow(const char* key, FutureMarketData& data);
	bool updateLastRow(const char* key, FutureMarketData& data);
	bool onStop();

private:
	const char* getName() const;
    void cacheRawMarketData(const FutureMarketData* data);
    void parseSrvConfig(const char* file);
	const char* convertStrNum(char* dest, double num, int reserved = 0);
	bool printMarketData(const FutureMarketData* data, bool completed, char* buffer);

private:
    redisContext* m_redisCtx;
    
    string m_configFileName;
    char m_srvIp[64];
    int  m_srvPort;
    int  m_timeout;

	char m_strBuffer[1024];

	std::mutex m_apiLock;

	ControllerImpl* m_controller;
};

} // namespace MarketHub

#endif // CACHE_REDIS_H