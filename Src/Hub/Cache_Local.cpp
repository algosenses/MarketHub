#include "ControllerImpl.h"
#include "Cache_Local.h"

using namespace MarketHub;

#define STR_BUF_LEN   (32)

static char strOpenPrice[STR_BUF_LEN] = { 0 };
static char strHighestPrice[STR_BUF_LEN] = { 0 };
static char strLowestPrice[STR_BUF_LEN] = { 0 };
static char strClosePrice[STR_BUF_LEN] = { 0 };
static char strOpenInterest[STR_BUF_LEN] = { 0 };
static char strTradingDay[STR_BUF_LEN] = { 0 };
static char strUpdateTime[STR_BUF_LEN] = { 0 };
static char strUpdateMs[STR_BUF_LEN] = { 0 };
static char strVolume[STR_BUF_LEN] = { 0 };
static char strUpperLimitPrice[STR_BUF_LEN] = { 0 };
static char strLowerLimitPrice[STR_BUF_LEN] = { 0 };
static char strLastPrice[STR_BUF_LEN] = { 0 };
static char strBidPrice1[STR_BUF_LEN] = { 0 };
static char strAskPrice1[STR_BUF_LEN] = { 0 };

Cache_Local::Cache_Local(ControllerImpl* controller)
	: m_controller(controller)
{
}

const char* Cache_Local::getName() const
{
	return "Cache_Local";
}

bool Cache_Local::onStart()
{
	return true;
}

const char* Cache_Local::convertStrNum(char *dest, double num, int reserved)
{
	if (num == DBL_MAX) {
		return NULL;
	}

	memset(dest, 0, STR_BUF_LEN);
	sprintf_s(dest, STR_BUF_LEN, "%.8f", num);

	char *dotPtr = strchr(dest, '.');
	if (dotPtr == NULL) {
		return dest;
	}

	int dotIdx = dotPtr - dest;
	int stopPos = dotIdx + reserved + 1;

	int nLen = strlen(dest);
	while (nLen >= stopPos) {
		nLen--;
		if (dest[nLen] == '.') {
			dest[nLen] = '\0';
			break;
		}
		else if (dest[nLen] == '0') {
			dest[nLen] = '\0';
		}
		else {
			break;
		}
	}

	return dest;
}

bool Cache_Local::printMarketData(const FutureMarketData* data, char* buffer, int size)
{
	if (data->openPrice == DBL_MAX ||
		data->highestPrice == DBL_MAX ||
		data->lowestPrice == DBL_MAX ||
		data->upperLimitPrice == DBL_MAX ||
		data->lowerLimitPrice == DBL_MAX ||
		data->lastPrice == DBL_MAX ||
		data->bidPrice1 == DBL_MAX ||
		data->askPrice1 == DBL_MAX ||
		data->openInterest == DBL_MAX) {
		return false;
	}

	convertStrNum(strOpenPrice, data->openPrice);
	convertStrNum(strHighestPrice, data->highestPrice);
	convertStrNum(strLowestPrice, data->lowestPrice);
	convertStrNum(strUpperLimitPrice, data->upperLimitPrice);
	convertStrNum(strLowerLimitPrice, data->lowerLimitPrice);
	convertStrNum(strLastPrice, data->lastPrice);
	convertStrNum(strBidPrice1, data->bidPrice1);
	convertStrNum(strAskPrice1, data->askPrice1);
	convertStrNum(strOpenInterest, data->openInterest);

	_snprintf_s(buffer, size, _TRUNCATE, "%d,%d,%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%d,%d,%d,%s\r\n",
		data->tradingDay,
		data->updateTime,
		data->updateMillisec,
		data->instrument,
		strOpenPrice,
		strHighestPrice,
		strLowestPrice,
		strUpperLimitPrice,
		strLowerLimitPrice,
		strLastPrice,
		strBidPrice1,
		strAskPrice1,
		data->bidVolume1,
		data->askVolume1,
		data->volume,
		strOpenInterest);

	return true;
}

void Cache_Local::cacheRawMarketData(const FutureMarketData* data)
{
	
}

void Cache_Local::storeBars(const char*instrument, int resolution, int interval, const vector<FutureMarketData>& data)
{
    char key[256] = { 0 };
    if (!m_controller->getKey(key, instrument, interval, resolution)) {
        return;
    }

    m_barDataMapLock.lock();
    auto it = m_barDataMap.find(key);
    if (it == m_barDataMap.end()) {
        it = m_barDataMap.insert(std::make_pair(key, std::deque<FutureMarketData>())).first;
    }

    for (size_t i = 0; i < data.size(); i++) {
        it->second.push_front(data[i]);
    }
    m_barDataMapLock.unlock();
}

void Cache_Local::storeTicks(const char*instrument, const vector<FutureMarketData>& data)
{
    char key[256] = { 0 };
    if (!m_controller->getKey(key, instrument, 1, Resolution::TICK)) {
        return;
    }

    m_tickDataMapLock.lock();
    auto it = m_tickDataMap.find(key);
    if (it == m_tickDataMap.end()) {
        it = m_tickDataMap.insert(std::make_pair(key, std::deque<FutureMarketData>())).first;
    }

    for (size_t i = 0; i < data.size(); i++) {
        it->second.push_front(data[i]);
    }
    m_tickDataMapLock.unlock();
}

void Cache_Local::storeData(const char*instrument, int resolution, int interval, const vector<FutureMarketData>& data)
{
    if (resolution == Resolution::TICK) {
        return storeTicks(instrument, data);
    } else {
        return storeBars(instrument, resolution, interval, data);
    }
}

int Cache_Local::retrieveBars(const char* instrument, int resolution, int interval, int num, vector<FutureMarketData>& rows)
{
    char key[256] = { 0 };
    if (!m_controller->getKey(key, instrument, interval, resolution)) {
        return 0;
    }

    m_barDataMapLock.lock();

    auto& it = m_barDataMap.find(key);
    if (it == m_barDataMap.end()) {
        m_barDataMapLock.unlock();
        return 0;
    }

    std::deque<FutureMarketData>& data = it->second;
    int count = 0;
    int idx = data.size() - 1;
    for (; count < num && count < data.size(); count++) {
        rows.push_back(data[idx--]);
    }

    m_barDataMapLock.unlock();

    return count;
}

int Cache_Local::retrieveTicks(const char* instrument, int num, vector<FutureMarketData>& rows)
{
    char key[256] = { 0 };
    if (!m_controller->getKey(key, instrument, 1, Resolution::TICK)) {
        return 0;
    }

    m_tickDataMapLock.lock();

    auto& it = m_tickDataMap.find(key);
    if (it == m_tickDataMap.end()) {
        m_tickDataMapLock.unlock();
        return 0;
    }

    std::deque<FutureMarketData>& data = it->second;
    int count = 0;
    int idx = data.size() - 1;
    for (; count < num && count < data.size(); count++) {
        rows.push_back(data[idx--]);
    }

    m_tickDataMapLock.unlock();

    return count;
}

int Cache_Local::retrieveData(const char*instrument, int resolution, int interval, int num, vector<FutureMarketData>& rows)
{
    if (resolution == Resolution::TICK) {
        return retrieveTicks(instrument, num, rows);
    } else {
        return retrieveBars(instrument, resolution, interval, num, rows);
    }

    return 0;
}

bool Cache_Local::appendNewBarRow(const char* key, const FutureMarketData& row)
{
	if (key == nullptr || key[0] == '\0') {
		return false;
	}

    m_barDataMapLock.lock();
	auto& it = m_barDataMap.find(key);
	if (it == m_barDataMap.end()) {
		it = m_barDataMap.insert(std::make_pair(key, std::deque<FutureMarketData>())).first;
	}

	it->second.push_back(row);
    m_barDataMapLock.unlock();

	return true;
}

bool Cache_Local::updateLastBarRow(const char* key, const FutureMarketData& row)
{
	if (key == nullptr || key[0] == '\0') {
		return false;
	}

    m_barDataMapLock.lock();
	auto& it = m_barDataMap.find(key);
	if (it == m_barDataMap.end()) {
        m_barDataMapLock.unlock();
		return false;
	}

	it->second.back() = row;
    m_barDataMapLock.unlock();

	return true;
}

bool Cache_Local::appendNewTickRow(const char*instrument, const FutureMarketData& row)
{
    char key[256] = { 0 };
    if (!m_controller->getKey(key, instrument, 1, Resolution::TICK)) {
        return 0;
    }

    m_tickDataMapLock.lock();
    auto& it = m_tickDataMap.find(key);
    if (it == m_tickDataMap.end()) {
        it = m_tickDataMap.insert(std::make_pair(key, std::deque<FutureMarketData>())).first;
    }

    it->second.push_back(row);
    m_tickDataMapLock.unlock();

    return true;
}

bool Cache_Local::onStop()
{
	return true;
}
