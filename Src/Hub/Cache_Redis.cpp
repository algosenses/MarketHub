#include <win32_hiredis.h>
#include <config.h>
#include "ControllerImpl.h"
#include "Cache_Redis.h"

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

Cache_Redis::Cache_Redis(ControllerImpl* controller)
	: m_controller(controller)
{
    m_redisCtx = NULL;
    m_configFileName = "MarketHub.ini";
	
}

const char* Cache_Redis::getName() const
{
	return "Cache_Redis";
}

void Cache_Redis::parseSrvConfig(const char* file)
{
    rude::Config config;
    if (config.load(file)) {
        config.setSection("Redis");
        const char* s = config.getStringValue("IP");
        if (s) {
            strcpy(m_srvIp, s);
        }
        m_srvPort = config.getIntValue("Port");
        m_timeout = config.getIntValue("Timeout");
    }
}

bool Cache_Redis::onStart()
{

    strcpy(m_srvIp, "127.0.0.1");
	m_srvPort = 6379;
	m_timeout = 5;

	parseSrvConfig(m_configFileName.c_str());

	if (m_redisCtx == NULL) {
		struct timeval timeout;
		timeout.tv_sec = m_timeout;
		timeout.tv_usec = 0;
		m_redisCtx = redisConnectWithTimeout(m_srvIp, m_srvPort, timeout);

		if (m_redisCtx->err) {
			redisFree(m_redisCtx);
			m_redisCtx = NULL;
			m_controller->sendLogMsg(LOG_INFO, "%s: Connect to redisServer[%s:%d] failed.", getName(), m_srvIp, m_srvPort);
			return false;
		}

		m_controller->sendLogMsg(LOG_INFO, "%s: Connect to redisServer[%s:%d] success.", getName(), m_srvIp, m_srvPort);

		m_apiLock.lock();
		redisReply* reply;
		reply = (redisReply*)redisCommand(m_redisCtx, "FLUSHALL");
		freeReplyObject(reply);
		m_apiLock.unlock();
	}

	return true;
}

const char* Cache_Redis::convertStrNum(char *dest, double num, int reserved)
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

bool Cache_Redis::printMarketData(const FutureMarketData* data, bool completed, char* buffer)
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

#if 0
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
#else
    convertStrNum(strOpenPrice, data->openPrice);
    convertStrNum(strHighestPrice, data->highestPrice);
    convertStrNum(strLowestPrice, data->lowestPrice);
    convertStrNum(strClosePrice, data->closePrice);
    convertStrNum(strOpenInterest, data->openInterest);
    _itoa(data->tradingDay, strTradingDay, 10);
    _itoa(data->updateTime, strUpdateTime, 10);
    _itoa(data->volume, strVolume, 10);
    _itoa(data->updateMillisec, strUpdateMs, 10);

    if (completed) {
        sprintf(buffer, "%s,%s,%s,%s,%s,%s,%s,%s",
            strTradingDay,
            strUpdateTime,
            strOpenPrice,
            strHighestPrice,
            strLowestPrice,
            strClosePrice,
            strVolume,
            strOpenInterest);
    } else {
        sprintf(buffer, "*%s,%s,%s,%s,%s,%s,%s,%s",
            strTradingDay,
            strUpdateTime,
            strOpenPrice,
            strHighestPrice,
            strLowestPrice,
            strClosePrice,
            strVolume,
            strOpenInterest);
    }
#endif

	return true;
}

void Cache_Redis::cacheRawMarketData(const FutureMarketData* data)
{
	if (m_redisCtx == NULL) {
		return;
	}

	m_apiLock.lock();

    char key[128] = { 0 };
    sprintf(key, "%s-0-TICK", data->instrument);

    printMarketData(data, data->completed, m_strBuffer);

    redisReply* reply;
    reply = (redisReply*)redisCommand(m_redisCtx, "LPUSH %s %s", key, m_strBuffer);
    freeReplyObject(reply);

	m_apiLock.unlock();
}

void Cache_Redis::storeData(const char* key, vector<FutureMarketData>& data)
{
	if (m_redisCtx == NULL) {
		return;
	}

	int cmd = 0;

	SYSTEMTIME lt;
	GetLocalTime(&lt);
	int date = lt.wYear * 10000 + lt.wMonth * 100 + lt.wDay;
	int time = lt.wHour * 10000 + lt.wMinute * 100 + lt.wSecond;

	m_apiLock.lock();

	for (size_t i = 0; i < data.size(); i++) {
		FutureMarketData& d = data[i];

		// filter data with invalid timestamp
		if (d.tradingDay > date) {
			continue;
		} else if (d.tradingDay == date && d.updateTime > time) {
			continue;
		}

        printMarketData(&d, true, m_strBuffer);

		redisAppendCommand(m_redisCtx, "RPUSH %s %s", key, m_strBuffer);
		cmd++;
	}

	while (cmd--) {
		redisReply* reply = NULL;
		if (REDIS_OK != redisGetReply(m_redisCtx, (void**)&reply)) {
			m_controller->sendLogMsg(LOG_ERROR, "%s: Get reply error", getName());
		}

		if (reply) {
			freeReplyObject(reply);
		}
	}

	m_apiLock.unlock();
}

int Cache_Redis::retrieveData(const char* key, int num, vector<string>& rows)
{
	if (m_redisCtx == NULL) {
		return 0;
	}

	m_apiLock.lock();

	redisReply* reply = (redisReply*)redisCommand(m_redisCtx, "LRANGE %s 0 %d", key, num - 1);
	if (reply == NULL) {
		return 0;
	}

	int count = reply->elements;
	if (reply->type == REDIS_REPLY_ARRAY) {
		for (int i = count - 1; i >= 0; i--) {
			char str[512];
			strncpy(str, reply->element[i]->str, sizeof(str)-1);
			rows.push_back(str);
		}
	}

	freeReplyObject(reply);

	m_apiLock.unlock();

	return count;
}

bool Cache_Redis::appendNewRow(const char* key, FutureMarketData& data)
{
	if (m_redisCtx == NULL) {
		return false;
	}

	m_apiLock.lock();

    printMarketData(&data, data.completed, m_strBuffer);

	redisReply* reply;
	reply = (redisReply*)redisCommand(m_redisCtx, "LPUSH %s %s", key, m_strBuffer);
	if (NULL == reply) {
		m_apiLock.unlock();
		return false;
	}

	if (!(reply->type == REDIS_REPLY_STATUS && !strcmp(reply->str, "OK") == 0)) {
		freeReplyObject(reply);
		m_apiLock.unlock();
		return false;
	}

	freeReplyObject(reply);

	m_apiLock.unlock();

	return true;
}

bool Cache_Redis::updateLastRow(const char* key, FutureMarketData& data)
{
	if (m_redisCtx == NULL) {
		return false;
	}

	m_apiLock.lock();

    printMarketData(&data, data.completed, m_strBuffer);

	redisReply* reply;
	reply = (redisReply*)redisCommand(m_redisCtx, "LSET %s 0 %s", key, m_strBuffer);
	
	if (NULL == reply) {
		m_apiLock.unlock();
		return false;
	}

	if (!(reply->type == REDIS_REPLY_STATUS && !strcmp(reply->str, "OK") == 0)) {
		freeReplyObject(reply);
		m_apiLock.unlock();
		return false;
	}

	freeReplyObject(reply);
	
	m_apiLock.unlock();

	return true;
}


bool Cache_Redis::onStop()
{
    if (m_redisCtx) {
        redisFree(m_redisCtx);
        m_redisCtx = NULL;
    }

    return true;
}
