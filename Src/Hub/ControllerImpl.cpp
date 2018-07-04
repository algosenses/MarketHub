#include <vector>
#include "config.h"
#include "Runtime.h"
#include "ControllerImpl.h"
#include "Composer.h"
#include "TinyCrypt.h"

#if defined _WIN32
#include <Windows.h>
#define LIBRARY_POINTER_TYPE     HMODULE
#define GET_PROC_ADDR            GetProcAddress
#define LOAD_LIBRARY(X)          LoadLibrary(X)
#define FREE_LIBRARY             FreeLibrary
#else
#include <dlfcn.h>
#define LIBRARY_POINTER_TYPE     void*
#define GET_PROC_ADDR            dlsym
#define LOAD_LIBRARY(X)          dlopen(X, RTLD_NOW)
#define FREE_LIBRARY             dlclose
#endif

using std::vector;

namespace MarketHub
{

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

ControllerImpl::ControllerImpl()
	: m_server(this)
	, m_configFileName("MarketHub.ini")
	, m_isRunning(false)
//	, m_cacheRedis(this)
	, m_cacheLocal(this)
	, m_dsMySql(this)
    , m_dsCsv(this)
{
    m_srvEnable = false;
    m_logger = Logger::instance();

	m_subscribeAllMarketData = true;

    m_evtHub.setEventListener(this);
    m_evtHub.startDispatchThread();
    std::this_thread::yield();
}

ControllerImpl::~ControllerImpl()
{
	sendLogMsg(LOG_INFO, "Release...");

    Message msg = { 0 };
    msg.header.type = MsgType_Invalid;

    m_evtHub.sendEvent(&msg);
    m_evtHub.stop();

    for (auto& runtime : m_runtimes) {
        if (runtime != nullptr) {
            delete runtime;
        }
    }
    m_runtimes.clear();

    if (m_logger) {
        m_logger->release();
        m_logger = nullptr;
    }
}

bool ControllerImpl::initialize()
{
    sendLogMsg(LOG_INFO, "+=================================================+");
	sendLogMsg(LOG_INFO, "*  Market Data Hub [build: " __DATE__ " " __TIME__ "]  *");
	sendLogMsg(LOG_INFO, "+=================================================+");

    return true;
}

int ControllerImpl::loadAdaptor(const char* library)
{
    int adaptorId = 0;
    if (library == nullptr || library[0] == '\0') {
        return 0;
    }

    LIBRARY_POINTER_TYPE module = LOAD_LIBRARY(library);
    if (module == nullptr) {
        return 0;
    }

    CreateAdaptorProc* pfnCreateAdaptor;
    pfnCreateAdaptor = (CreateAdaptorProc*)GET_PROC_ADDR(module, "CreateAdaptor");
    if (pfnCreateAdaptor == nullptr) {
        FREE_LIBRARY(module);
        return 0;
    }

    Adaptor* adaptor = pfnCreateAdaptor();
    if (adaptor != nullptr) {
        Runtime* runtime = new Runtime();
        runtime->setAdaptorObject(adaptor);
        runtime->initialize(this);
        adaptorId = getNextRuntimeId();
        runtime->setId(adaptorId);
        adaptor->setRuntime(runtime);
        
        m_runtimes.push_back(runtime);

        adaptor->onCreate();

        m_logger->sendLog(LOG_INFO, "Load adaptor [ID: %d, TYPE: %-7s NAME: %s]", 
            runtime->getId(),
            adaptor->type() == Adaptor::SOURCE ? "Source," : "Sink,", 
            adaptor->getName());
    }

    return adaptorId;
}

bool ControllerImpl::sendLogMsg(int level, const char* msgFmt, ...)
{
	char sMsg[1024];
	int bufLen = sizeof(sMsg);
	memset(sMsg, 0, bufLen);

	va_list args;
	va_start(args, msgFmt);
	int len = vsnprintf(NULL, 0, msgFmt, args);
	if (len > 0) {
		len = (len >= bufLen - 1 ? bufLen - 1 : len);
		vsnprintf(sMsg, len + 1, msgFmt, args);
	}
	va_end(args);

	if (len > 0) {
		m_logger->sendLog(level, sMsg);

        Message msg = { 0 };
        msg.header.type = MsgType_Log;

        SYSTEMTIME lt;
        GetLocalTime(&lt);

        msg.body.log.date = lt.wYear * 10000 + lt.wMonth * 100 + lt.wDay;
        msg.body.log.time = lt.wHour * 10000 + lt.wMinute * 100 + lt.wSecond;
        msg.body.log.ms = lt.wMilliseconds;
        msg.body.log.level = level;
        strncpy(msg.body.log.text, sMsg, sizeof(msg.body.log.text) - 1);
        sendMessage(msg);

		return true;
	}

	return false;
}

bool ControllerImpl::getKey(char* key, const char* instrument, int interval, int resolution)
{
	if (key == nullptr) {
		return false;
	}

	if (instrument == nullptr || instrument[0] == '\0') {
		return false;
	}

	char* resName = nullptr;
	switch (resolution) {
    case TICK:
        resName = "TICK";
        break;
	case SECOND:
		resName = "SECOND";
		break;
	case MINUTE:
		resName = "MINUTE";
		break;
	case HOUR:
		resName = "HOUR";
		break;
	case DAY:
		resName = "DAY";
		break;
	case WEEK:
		resName = "WEEK";
		break;
	default:
		return false;
	}

	sprintf(key, "%s-%d-%s", instrument, interval, resName);

	return true;
}

void ControllerImpl::subscribeMsg(int msgType, Subscriber* subscriber)
{
	auto it = m_subscribers.find(msgType);
	if (it == m_subscribers.end()) {
		it = m_subscribers.insert(std::make_pair(msgType, vector<Subscriber*>())).first;
	}

	vector<Subscriber*>& subscribers = it->second;
	bool found = false;
	for (auto& s : subscribers) {
		if (s == subscriber) {
			found = true;
			break;
		}
	}

	if (!found) {
		subscribers.push_back(subscriber);
	}
}

void ControllerImpl::run()
{
	m_dsMySql.onStart();
    m_dsCsv.onStart();
//	m_cacheRedis.onStart();
	m_cacheLocal.onStart();

	// parse trading hours first.
	parseTradingHours("TradingHours.ini");
	parseConfig(m_configFileName.c_str());

	startCaching();

    if (m_srvEnable) {
        m_server.start();
    }

	m_isRunning = true;

    for (auto& runtime : m_runtimes) {
        if (runtime != nullptr) {
            runtime->start();
        }
    }
	
}

void ControllerImpl::stop()
{
	m_isRunning = false;

    for (auto& runtime : m_runtimes) {
        if (runtime != nullptr) {
            runtime->stop();
        }
    }

    m_runtimes.clear();

	for (auto& item : m_composers) {
		for (auto& composer : item.second) {
			if (composer.composer != nullptr) {
				delete composer.composer;
				composer.composer = nullptr;
			}
		}
		item.second.clear();
	}
	m_composers.clear();

	m_server.stop();
//	m_cacheRedis.onStop();
	m_cacheLocal.onStop();
	m_dsMySql.onStop();
    m_dsCsv.onStop();
}

bool ControllerImpl::decrypt(const char* enc, const char* key, char* plaintext)
{
#if ENCRYPT_PASSWORD
    TinyCrypt::CryptObject cryptObj((unsigned char*)key);
    std::string raw = cryptObj.decrypt(enc);

    strcpy(plaintext, raw.c_str());

    return true;
#else
    strcpy(plaintext, enc);
    return true;
#endif
}

bool ControllerImpl::sendMessage(const Message& msg)
{
    if (!m_evtHub.isRunning()) {
        return false;
    }

    return m_evtHub.sendEvent(&msg);
}

void ControllerImpl::sendAdaptorStatus(int id, int type, const char* name, int status, const char* extra)
{
    Message msg = { 0 };
    msg.header.type = MsgType_AdaptorStatus;
    msg.body.adaptorStatus.id = id;
    msg.body.adaptorStatus.type = type;
    if (name && name[0] != '\0') {
        strncpy(msg.body.adaptorStatus.name, name, sizeof(msg.body.adaptorStatus.name) - 1);
    }
    msg.body.adaptorStatus.status = status;
    if (extra && extra[0] != '\0') {
        strncpy(msg.body.adaptorStatus.extraData, extra, sizeof(msg.body.adaptorStatus.extraData) - 1);
    }

    sendMessage(msg);
}

void ControllerImpl::sendMarketData(int adaptorId, int type, void* data)
{
    if (adaptorId <= 0) {
        return;
    }

    if (type != MsgType_FutureMarketData &&
        type != MsgType_StockMarketData &&
        type != MsgType_OptionMarketData) {
        return;
    }

    Message msg = { 0 };
    msg.header.type = type;
    msg.header.source = adaptorId;
    if (type == MsgType_FutureMarketData) {
        memcpy(&msg.body.futureData, data, sizeof(FutureMarketData));
    }

    sendMessage(msg);
}

bool ControllerImpl::onEvent(const void* evt)
{
    return dispatchEvent((Message *)evt);
}

// CORE EVENT LOOP.
bool ControllerImpl::dispatchEvent(const Message* pMsg)
{
    int type = pMsg->header.type;
    if (type == MsgType_Invalid) {
        return false;
    }

	auto& it = m_subscribers.find(type);
	vector<Subscriber*>& subscribers = it->second;
	for (auto& s : subscribers) {
		if (s) {
			s->onMessage(pMsg);
		}
	}

    return true;
}

void ControllerImpl::onMessage(const Message* msg)
{
	if (!m_isRunning) {
		return;
	}

	int type = msg->header.type;
	if (type != MsgType_FutureMarketData) {
		return;
	}

	FutureMarketData* data = (FutureMarketData*)&(msg->body.futureData);
	if (!m_subscribeAllMarketData) {
		if (m_subscriptions.find(data->instrument) != m_subscriptions.end()) {
			cacheData(data);
		}
	} else {
        cacheData(data);
	}

	return;
}

unsigned long ControllerImpl::getNextRuntimeId()
{
    return ++m_nextRtId;
}

const char* ControllerImpl::convertStrNum(char *dest, double num, int reserved)
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

bool ControllerImpl::printBarData(char* out, const FutureMarketData& d)
{
    convertStrNum(strOpenPrice, d.openPrice);
    convertStrNum(strHighestPrice, d.highestPrice);
    convertStrNum(strLowestPrice, d.lowestPrice);
    convertStrNum(strClosePrice, d.closePrice);
    convertStrNum(strOpenInterest, d.openInterest);
    _itoa(d.tradingDay, strTradingDay, 10);
    _itoa(d.updateTime, strUpdateTime, 10);
    _itoa(d.volume, strVolume, 10);
    _itoa(d.updateMillisec, strUpdateMs, 10);

    if (d.completed) {
        sprintf(out, "%s,%s,%s,%s,%s,%s,%s,%s",
            strTradingDay,
            strUpdateTime,
            strOpenPrice,
            strHighestPrice,
            strLowestPrice,
            strClosePrice,
            strVolume,
            strOpenInterest);
    } else {
        sprintf(out, "*%s,%s,%s,%s,%s,%s,%s,%s",
            strTradingDay,
            strUpdateTime,
            strOpenPrice,
            strHighestPrice,
            strLowestPrice,
            strClosePrice,
            strVolume,
            strOpenInterest);
    }

    return true;
}

bool ControllerImpl::printTickData(char* out, const FutureMarketData& d)
{
    convertStrNum(strOpenPrice, d.openPrice);
    convertStrNum(strHighestPrice, d.highestPrice);
    convertStrNum(strLowestPrice, d.lowestPrice);
    convertStrNum(strLastPrice, d.lastPrice);
    convertStrNum(strOpenInterest, d.openInterest);
    convertStrNum(strBidPrice1, d.bidPrice1);
    convertStrNum(strAskPrice1, d.askPrice1);
    _itoa(d.tradingDay, strTradingDay, 10);
    _itoa(d.updateTime, strUpdateTime, 10);
    _itoa(d.volume, strVolume, 10);
    _itoa(d.updateMillisec, strUpdateMs, 10);

    sprintf(out, "%s,%s,%d,%s,%s,%s,%s,%s,%s,%s,%d,%s,%d",
        strTradingDay,
        strUpdateTime,
        d.updateMillisec,
        strOpenPrice,
        strHighestPrice,
        strLowestPrice,
        strLastPrice,
        strVolume,
        strOpenInterest,
        strBidPrice1,
        d.bidVolume1,
        strAskPrice1,
        d.askVolume1);

    return true;
}

bool ControllerImpl::printMarketData(char* out, int resolution, const FutureMarketData& d)
{
    if (resolution == Resolution::TICK) {
        return printTickData(out, d);
    } else {
        return printBarData(out, d);
    }
}

string ControllerImpl::getSymbol(const char* instrument)
{
	char inst[64] = { 0 };
	int pos = 0;
	const char *p = instrument;
	while (*p == ' ' || *p == '\t') p++;
	while (isalpha(*p)) {
		inst[pos] = *p;
		pos++;
		p++;
	}

	if (!isdigit(*p)) return string();
//	while (isdigit(*p)) p++;
//	if (*p != '\0') return string();

	return inst;
}

void ControllerImpl::splitHours(char* input, TradingHours& hours)
{
	char* delim = "|";
	char* p = strtok(input, delim);
	while (p) {
		char* t = strchr(p, '-');
		if (t) {
			*t = '\0';
			int begin = atoi(p);
			int end = atoi(++t);
			TradingHour session;
			sprintf(session.begin, "%02d:%02d:%02d", begin / 10000, begin % 10000 / 100, begin % 100);
			sprintf(session.end, "%02d:%02d:%02d", end / 10000, end % 10000 / 100, end % 100);
			hours.push_back(session);
		}

		p = strtok(NULL, delim);
	}
}

void ControllerImpl::splitContracts(char* input, vector<string>& contracts)
{
	char* delim = ",";
	char* p = strtok(input, delim);
	while (p) {
		contracts.push_back(p);
		p = strtok(NULL, delim);
	}
}

void ControllerImpl::parseTradingHours(const char* file)
{
	rude::Config config;
	if (!config.load(file)) {
		return;
	}

	config.setSection("TradingHours");
	int num = config.getNumDataMembers();
	if (num <= 0) {
		return;
	}

	char line[256];

	for (int i = 0; i < num; i++) {
		const char *l = config.getDataNameAt(i);
		if (l) {
			strncpy(line, l, sizeof(line));
			char hoursLine[256] = { 0 };
			char contractsLine[256] = { 0 };
			char* p = strchr(line, ':');
			if (!p) {
				continue;
			}
			*p = '\0';

			strncpy(hoursLine, line, sizeof(hoursLine));
			strncpy(contractsLine, ++p, sizeof(contractsLine));

			TradingHours hours;
			vector<string> contracts;
			splitHours(hoursLine, hours);
			splitContracts(contractsLine, contracts);
			if (hours.size() > 0 && contracts.size() > 0) {
				for (size_t i = 0; i < contracts.size(); i++) {
					m_tradingHours.insert(std::make_pair(contracts[i], hours));
				}
			}
		}
	}
}

void ControllerImpl::parsePreloadConfigLine(const char* configLine)
{
	char line[512] = { 0 };
	strncpy(line, configLine, sizeof(line));

	// instrument,interval,resolution,number
	enum FieldIndex {
		Instrument = 0,
		Interval,
		Resolution,
		Number,
        Source,
		End
	};

	char instrument[64] = { 0 };
	int  interval = 1;
	int  resolution = MINUTE;
	int  number = 0;
    int  ds = DataSrc_MYSQL;

	char* token;
	int fieldIdx = FieldIndex::Instrument;
	token = strtok(line, ",");
	while (token) {
		switch (fieldIdx) {
		case FieldIndex::Instrument:
			strncpy(instrument, token, sizeof(instrument));
			break;
		case FieldIndex::Interval:
			interval = atoi(token);
			break;
		case FieldIndex::Resolution:
			if (!strcmp(token, "MINUTE")) {
				resolution = MINUTE;
			}
			else if (!strcmp(token, "SECOND")) {
				resolution = SECOND;
			}
			else if (!strcmp(token, "HOUR")) {
				resolution = HOUR;
			}
			break;
		case FieldIndex::Number:
			number = atoi(token);
			break;
        case FieldIndex::Source:
            if (!_stricmp(token, "MYSQL")) {
                ds = DataSrc_MYSQL;
            } else if (!_stricmp(token, "CSV")) {
                ds = DataSrc_CSV;
            } else {
                ds = DataSrc_Unknown;
            }
            break;
		}

		token = strtok(NULL, ",");
		fieldIdx++;
	}

    fieldIdx = FieldIndex::End;

	if (fieldIdx == FieldIndex::End) {
        m_instrumentDS.insert(std::make_pair(instrument, ds));

		string symbol = getSymbol(instrument);
		auto& it = m_tradingHours.find(symbol);
		if (it != m_tradingHours.end()) {
			TradingHours hours = it->second;
			PreloadDataConfig c;
			strcpy(c.instrument, instrument);
			c.resolution = resolution;
			c.interval = interval;
			c.number = number;
			c.tradingHours = hours;
            c.source = ds;
			m_preloadDataConfig.push_back(c);
		}
	}
}

void ControllerImpl::parseConfig(const char* file)
{
	rude::Config config;
	if (config.load(file)) {
        config.setSection("Server");
        m_srvEnable = config.getBoolValue("Enable");

		config.setSection("Preload");
		int num = config.getNumDataMembers();
		if (num > 0) {
			for (int i = 0; i < num; i++) {
				const char *line = config.getDataNameAt(i);
				if (line != NULL) {
					parsePreloadConfigLine(line);
				}
			}
		}
	}
}

bool ControllerImpl::installComposer(const char* instrument, int resolution, int interval, TradingHours& hours)
{
	if (instrument == nullptr || instrument[0] == '\0') {
		return false;
	}

	ComposerContex ctx;
	strcpy(ctx.instrument, instrument);
	ctx.interval = interval;
	ctx.resolution = resolution;
	if (!getKey(ctx.key, instrument, interval, resolution)) {
		return false;
	}

	auto& itor = m_composers.find(instrument);
	if (itor == m_composers.end()) {
		itor = m_composers.insert(std::make_pair(instrument, std::vector<ComposerContex>(0))).first;
		
		Composer* composer = new Composer();
		composer->init(instrument, resolution, interval, hours);
		ctx.composer = composer;

		itor->second.push_back(ctx);
	} else {
		std::vector<ComposerContex>& composers = itor->second;
		bool found = false;
		for (size_t i = 0; i < composers.size(); i++) {
			if (!strcmp(composers[i].instrument, instrument) &&
				composers[i].interval == interval &&
				composers[i].resolution == resolution) {
				found = true;
				break;
			}
		}

		if (!found) {
			Composer* composer = new Composer();
			composer->init(instrument, resolution, interval, hours);
			ctx.composer = composer;
			composers.push_back(ctx);
		}
	}

	return true;
}

void ControllerImpl::preloadHistData()
{
	sendLogMsg(LOG_INFO, "Preloading historical data....");

	for (size_t i = 0; i < m_preloadDataConfig.size(); i++) {
		PreloadDataConfig& c = m_preloadDataConfig[i];

		vector<FutureMarketData> rows;
		DateTime end;
        if (c.source == DataSrc_MYSQL) {
            m_dsMySql.retrieveData(c.instrument, c.resolution, c.interval, c.number, end, rows);
        } else if (c.source == DataSrc_CSV) {
            m_dsCsv.retrieveData(c.instrument, c.resolution, c.interval, c.number, end, rows);
        } else if (c.source == DataSrc_Unknown) {
            // load data from local first.
            m_dsCsv.retrieveData(c.instrument, c.resolution, c.interval, c.number, end, rows);
            if (rows.size() < c.number) {
                rows.clear();
                m_dsMySql.retrieveData(c.instrument, c.resolution, c.interval, c.number, end, rows);
            }
        }

		if (rows.size() > 0) {
//			m_cacheRedis.storeData(key, rows);
			m_cacheLocal.storeData(c.instrument, c.resolution, c.interval, rows);
		}

		installComposer(c.instrument, c.resolution, c.interval, c.tradingHours);
		m_subscriptions.insert(std::make_pair(c.instrument, 0));
	}
}

void ControllerImpl::startCaching()
{
	preloadHistData();

	subscribeMsg(MsgType_FutureMarketData, this);
}

void ControllerImpl::cacheData(const FutureMarketData* data)
{
    if (data->instrument[0] == '\0') {
        return;
    }

    // cache raw ticks.
    m_cacheLocal.appendNewTickRow(data->instrument, *data);

    // cache bars.
    doComposition(data);
}

void ControllerImpl::doComposition(const FutureMarketData* data)
{
	if (m_composers.find(data->instrument) == m_composers.end()) {
		auto& it = m_tradingHours.find(getSymbol(data->instrument));
		if (it != m_tradingHours.end()) {
			TradingHours hours = it->second;
			installComposer(data->instrument, Resolution::MINUTE, 1, hours);
		}
	}

	auto& itor = m_composers.find(data->instrument);
	if (itor != m_composers.end()) {
		for (ComposerContex& ctx : itor->second) {
			Composer* composer = ctx.composer;

            int state = composer->appendMarketData(*data);
			if (state == Composer::RETURN_ERROR) {
				continue;
			}

			FutureMarketData d = composer->getCompositedData();
            if (ctx.hasUncompletedRow) {
                // update the last bar.
//                m_cacheRedis.updateLastRow(ctx.key, d);
                m_cacheLocal.updateLastBarRow(ctx.key, d);
                if (d.completed) {
                    ctx.hasUncompletedRow = false;
                }
            } else {
                // insert a new bar
//                m_cacheRedis.appendNewRow(ctx.key, d);
                m_cacheLocal.appendNewBarRow(ctx.key, d);
                if (!d.completed) {
                    ctx.hasUncompletedRow = true;
                } else {
                    ctx.hasUncompletedRow = false;
                }
            }
		}
	}

	return;
}

int ControllerImpl::retrieveData(const char* instrument, int resolution, int interval, int num, vector<string>& rows)
{
    if (instrument == nullptr ||
        instrument[0] == '\0' || 
        isblank(instrument[0]) ||
        strlen(instrument) > INSTRUMENT_NAME_MAX_LEN) {
        return 0;
    }

    if (num <= 0) {
        return 0;
    }

    if (m_tradingHours.find(getSymbol(instrument)) == m_tradingHours.end()) {
        return false;
    }

	vector<FutureMarketData> cacheData;
    vector<FutureMarketData> histData;
	// Read data from live cache firstly.
//	int cacheNum = m_cacheRedis.retrieveData(key, num, cacheData);
    int cacheNum = m_cacheLocal.retrieveData(instrument, resolution, interval, num, cacheData);
	int persNum = 0;
	int remain = num - cacheNum;

	int ret = 0;

	if (remain > 0) {
		// If cache has no enough data, then retrieve data from persistence and
		// refresh historical cache.

		DateTime end;
		if (cacheData.size() > 0) {
#if 0
			char line[1024] = { 0 };
			strcpy(line, cacheData.front().c_str());
			char* token;
			token = strtok(line, ",");
			int tokenIdx = 0;
			while (token) {
				if (tokenIdx == 0) {
					end.date = atoi(token);
				} else if (tokenIdx == 1) {
					end.time = atoi(token);
					break;
				}
				token = strtok(NULL, ",");
				tokenIdx++;
			}
#else
            end.date = cacheData.back().tradingDay;
            end.time = cacheData.back().updateTime;
            end.ms   = cacheData.back().updateMillisec;
#endif
		}

        int ds = DataSrc_MYSQL;
        const auto& it = m_instrumentDS.find(instrument);
        if (it != m_instrumentDS.end()) {
            ds = it->second;
        }

        if (ds == DataSrc_MYSQL) {
            m_dsMySql.retrieveData(instrument, resolution, interval, remain, end, histData);
        } else {
            m_dsCsv.retrieveData(instrument, resolution, interval, remain, end, histData);
        }

		persNum = histData.size();
		if (persNum > 0) {
//			m_cacheRedis.storeData(key, histData);
            m_cacheLocal.storeData(instrument, resolution, interval, histData);
		}
	}

    for (size_t i = 0; i < cacheData.size(); i++) {
        char line[512] = { 0 };
        printMarketData(line, resolution, cacheData[i]);
        rows.push_back(line);
    }
    ret += cacheData.size();

    for (size_t i = 0; i < histData.size(); i++) {
        char line[512] = { 0 };
        printMarketData(line, resolution, histData[i]);
        rows.push_back(line);
    }
    ret += persNum;

	auto& it = m_tradingHours.find(getSymbol(instrument));
	if (it != m_tradingHours.end()) {
		TradingHours hours = it->second;
		installComposer(instrument, resolution, interval, hours);
	}

	return ret;
}

} // namespace MarketHub