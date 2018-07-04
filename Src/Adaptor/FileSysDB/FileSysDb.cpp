#include <windows.h>
#include <iostream>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
#include <time.h>
#include "FileSysDb.h"

#define DIGIT_BUF_LEN   (64)

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

FileSysDb::FileSysDb()
{
    m_dataPath = "./Data";
    m_totalMarketDataNum = 0;
}

FileSysDb::~FileSysDb()
{
}

const char* FileSysDb::getName() const
{
    return "FileSysDB";
}

bool FileSysDb::onStart()
{
    subscribeAllMarketData(MsgType_FutureMarketData);

    notifyStatusChanged(ADAPTOR_RUNNING, nullptr);

    return true;
}

bool FileSysDb::onStop()
{
    if (m_mktDataQueue.unsafe_size() > 0) {
        FutureMarketData tmp;
        while (m_mktDataQueue.try_pop(tmp)) {
            saveMarketData(&tmp);
        }
    }

    return true;
}

const char* FileSysDb::convertStrNum(char *dest, double num, int reserved)
{
    if (num == DBL_MAX) {
        return NULL;
    }

    memset(dest, 0, DIGIT_BUF_LEN);
    sprintf_s(dest, DIGIT_BUF_LEN, "%.8f", num);

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
        } else if (dest[nLen] == '0') {
            dest[nLen] = '\0';
        } else {
            break;
        }
    }

    return dest;
}

// Example:
// date: 20140828
// time: 94704
long long FileSysDb::convertDateTime(int date, int time)
{
    int year = date / 10000;
    int month = (date % 10000) / 100;
    int day = date % 100;
    int hour = time / 10000;
    int min = (time % 10000) / 100;
    int sec = time % 100;

    struct tm utc;
    memset(&utc, 0, sizeof(utc));
    utc.tm_year = year;
    utc.tm_mon = month;
    utc.tm_mday = day;
    utc.tm_hour = hour;
    utc.tm_min = min;
    utc.tm_sec = sec;

    utc.tm_year -= 1900;
    --utc.tm_mon;

    struct timeval t_ = make_timeval(timegm(&utc), 0);

    return t_.tv_sec;
}

static char strOpenPrice[DIGIT_BUF_LEN] = { 0 };
static char strHighestPrice[DIGIT_BUF_LEN] = { 0 };
static char strLowestPrice[DIGIT_BUF_LEN] = { 0 };
static char strUpperLimitPrice[DIGIT_BUF_LEN] = { 0 };
static char strLowerLimitPrice[DIGIT_BUF_LEN] = { 0 };
static char strLastPrice[DIGIT_BUF_LEN] = { 0 };
static char strBidPrice1[DIGIT_BUF_LEN] = { 0 };
static char strAskPrice1[DIGIT_BUF_LEN] = { 0 };
static char strOpenInterest[DIGIT_BUF_LEN] = { 0 };
static char strTradingDay[DIGIT_BUF_LEN] = { 0 };
static char strUpdateTime[DIGIT_BUF_LEN] = { 0 };
static char strVolume[DIGIT_BUF_LEN] = { 0 };
static char strTurnover[DIGIT_BUF_LEN] = { 0 };
static char strUpdateMillisec[DIGIT_BUF_LEN] = { 0 };
static char strBidVolume1[DIGIT_BUF_LEN] = { 0 };
static char strAskVolume1[DIGIT_BUF_LEN] = { 0 };

bool FileSysDb::printMarketData(const FutureMarketData* data, char* buffer, int size)
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
    convertStrNum(strTurnover, data->turnover);
    _itoa(data->tradingDay, strTradingDay, 10);
    
    int hour, min, sec;
    hour = data->updateTime / 10000;
    min = (data->updateTime % 10000) / 100;
    sec = data->updateTime % 100;
    sprintf(strUpdateTime, "%02d:%02d:%02d", hour, min, sec);

    _itoa(data->volume, strVolume, 10);
    _itoa(data->updateMillisec, strUpdateMillisec, 10);
    _itoa(data->bidVolume1, strBidVolume1, 10);
    _itoa(data->askVolume1, strAskVolume1, 10);

    _snprintf_s(buffer, size, _TRUNCATE, "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\r\n",
        strTradingDay,
        strUpdateTime,
        strUpdateMillisec,
        strOpenPrice,
        strHighestPrice,
        strLowestPrice,
        strUpperLimitPrice,
        strLowerLimitPrice,
        strLastPrice,
        strBidPrice1,
        strAskPrice1,
        strBidVolume1,
        strAskVolume1,
        strVolume,
        strTurnover,
        strOpenInterest);
  
    return true;
}

bool FileSysDb::saveMarketData(const FutureMarketData* data)
{
    if (data->instrument[0] == '\0') {
        return false;
    }

    if (data->tradingDay == 0) {
        return false;
    }

    char key[256];
    char subPath[64];
    const char *p = data->instrument;
    int i = 0;
    while (p[i] != '\0' && !isdigit(p[i]) && i < sizeof(subPath)-1) {
        subPath[i] = p[i];
        i++;
    }
    subPath[i] = '\0';

    SYSTEMTIME lt;
    GetLocalTime(&lt);
    char localDate[64];
    sprintf_s(localDate, "%04d%02d%02d", lt.wYear, lt.wMonth, lt.wDay);
//    sprintf_s(key, "%s-%s.txt", data->InstrumentID, date);
    sprintf_s(key, "%s-%d", data->instrument, data->tradingDay);

    tbb::interface5::concurrent_hash_map<string, InstrTargetFile>::accessor a;
#ifdef _WIN32
    HANDLE hFile = INVALID_HANDLE_VALUE;
#else
    FILE *fp = NULL;
#endif

    long long lastMotifiedTime = 0;

    if (!m_instrFiles.find(a, key)) {
        char filename[512] = { 0 };
        // create data directory
        if (!m_dataPath.empty()) {
            if (_access(m_dataPath.c_str(), 0) != 0) {
                _mkdir(m_dataPath.c_str());
            }

            // All data placed in 'Tick' directory.
            sprintf(filename, "%s\\TICK", m_dataPath.c_str());
            if (_access(filename, 0) != 0) { _mkdir(filename); }

            // Exchange
            sprintf(filename, "%s\\%s", filename, data->exchange[0] == '\0' ? "Unknown" : data->exchange);
            if (_access(filename, 0) != 0) { _mkdir(filename); }

            // Symbol
            sprintf(filename, "%s\\%s", filename, subPath);
            if (_access(filename, 0) != 0) { _mkdir(filename); }

            // Instrument
            sprintf(filename, "%s\\%s", filename, data->instrument);
            if (_access(filename, 0) != 0) { _mkdir(filename); }
            
            sprintf_s(filename, 512, "%s\\%s-%s.csv", filename, key, localDate);
        } else {
            if (_access(subPath, 0) != 0) { _mkdir(subPath); }

            sprintf(filename, "%s\\%s", subPath, data->instrument);
            if (_access(filename, 0) != 0) { _mkdir(filename); }

            sprintf_s(filename, 512, "%s\\%s", filename, key);
        }

        bool existed = (_access(filename, 0) == 0);
        if (existed) {
            struct stat attrib;
            stat(filename, &attrib);
            lastMotifiedTime = attrib.st_mtime;
        }

#ifdef _WIN32
        hFile = CreateFile(filename,
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
#else
        fopen_s(&fp, filename, "a");
#endif
        if (hFile != INVALID_HANDLE_VALUE) {
            InstrTargetFile desc;
            desc.file = filename;
            desc.hFile = hFile;
            desc.lastMotifiedTime = lastMotifiedTime;
            m_instrFiles.insert(a, key);
            a->second = desc;
#if 1
            if (!existed) {
#ifdef _WIN32
                char buf[512];
                memset(buf, 0, sizeof(buf));
                sprintf_s(buf, "%s\r\n", MARKETDATA_CSV_HEADER);
                DWORD dwWritenSize = 0;
                BOOL bRet = false;
                bRet = WriteFile(hFile, buf, strlen(buf), &dwWritenSize, NULL);
#else
                fprintf(fp, "%s\n", TICK_FILE_FIRST_LINE_CONTENT);
#endif
            }
#endif
        } else {
            a.release();
            return false;
        }
    } else {
#ifdef _WIN32
        hFile = a->second.hFile;
#else
        fp = a->second.fp;
#endif
        lastMotifiedTime = a->second.lastMotifiedTime;
    }

    if (hFile == INVALID_HANDLE_VALUE) {
        a.release();
        return false;
    }

    memset(m_line, 0, sizeof(m_line));
    bool ret = printMarketData(data, m_line, sizeof(m_line));

    if (ret && strlen(m_line) > 0) {
        if (convertDateTime(data->tradingDay, data->updateTime) > lastMotifiedTime) {
            DWORD dwWritenSize = 0;
            BOOL bRet = false;
            bRet = WriteFile(hFile, m_line, strlen(m_line), &dwWritenSize, NULL);
            //fflush(fp);
        }
    }

    a.release();

    return true;
}

bool FileSysDb::handleMarketData(const FutureMarketData& futureData)
{
    m_mktDataQueue.push(futureData);
    m_totalMarketDataNum++;

    if (m_mktDataQueue.unsafe_size() > MARKETDATA_FLUSH_THRESHOLD) {
        FutureMarketData tmp;
        while (m_mktDataQueue.try_pop(tmp)) {
            saveMarketData(&tmp);
        }
    }

    return true;
}

bool FileSysDb::handleTimeSync(const TimeSync& sync)
{
    return true;
}

bool FileSysDb::onMarketData(int type, const void* data)
{
    if (type == MsgType_FutureMarketData) {
        FutureMarketData& d = *((FutureMarketData*)data);
        return handleMarketData(d);
    }

    return true;
}

bool FileSysDb::onTimeSync(const TimeSync& sync)
{
    handleTimeSync(sync);

    return true;
}

EXPORT_ADAPTOR(FileSysDb)