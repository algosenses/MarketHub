#ifndef CACHE_LOCAL_H
#define CACHE_LOCAL_H

#include <unordered_map>
#include <string>
#include <vector>
#include <deque>
#include <mutex>

#include "tbb/spin_mutex.h"
#include "MarketHub.h"

using std::vector;
using std::string;

namespace MarketHub
{

class Cache_Local
{
public:
	Cache_Local(ControllerImpl* controller);
	bool onStart();
	void storeData(const char*instrument, int resolution, int interval, const vector<FutureMarketData>& data);
    // Latest data at the front(rows[0])
	int  retrieveData(const char*instrument, int resolution, int interval, int num, vector<FutureMarketData>& rows);
	bool appendNewBarRow(const char* key, const FutureMarketData& row);
	bool updateLastBarRow(const char* key, const FutureMarketData& row);
    bool appendNewTickRow(const char*instrument, const FutureMarketData& row);
    bool onStop();

private:
	const char* getName() const;
    void cacheRawMarketData(const FutureMarketData* data);
	const char* convertStrNum(char* dest, double num, int reserved = 0);
	bool printMarketData(const FutureMarketData* data, char* buffer, int size);
    void storeBars(const char*instrument, int resolution, int interval, const vector<FutureMarketData>& data);
    void storeTicks(const char*instrument, const vector<FutureMarketData>& data);
    int  retrieveBars(const char*instrument, int resolution, int interval, int num, vector<FutureMarketData>& rows);
    int  retrieveTicks(const char*instrument, int num, vector<FutureMarketData>& rows);
private:
	char m_strBuffer[1024];

	std::unordered_map<string, std::deque<FutureMarketData> > m_barDataMap;
    std::unordered_map<string, std::deque<FutureMarketData> > m_tickDataMap;

    tbb::spin_mutex m_barDataMapLock;
    tbb::spin_mutex m_tickDataMapLock;

	ControllerImpl* m_controller;
};

} // namespace MarketHub

#endif // CACHE_LOCAL_H