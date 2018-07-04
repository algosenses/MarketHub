#ifndef FILE_SYS_DB_H
#define FILE_SYS_DB_H

#include <string>
#include "MarketHub.h"
#include "Adaptor.h"
#include "tbb/concurrent_queue.h"
#include "tbb/concurrent_hash_map.h"

using namespace MarketHub;

using std::string;

#define MARKETDATA_FLUSH_THRESHOLD    (200)
#define MARKETDATA_CSV_HEADER \
    "date,time,millisec,open,high,low,upperlimit,lowerlimit,last,bid,ask,bidvolume,askvolume,volume,turnover,openint"

class FileSysDb : public Adaptor
{
public:
    FileSysDb();
    ~FileSysDb();

    int type() const { return SINK; }
    const char* getName() const;
    bool onStart();
    bool onMarketData(int type, const void* data);
    bool onTimeSync(const TimeSync& sync);
    bool onStop();

private:
    long long convertDateTime(int date, int time);
    const char* convertStrNum(char* dest, double num, int reserved = 0);
    bool printMarketData(const FutureMarketData* data, char* buffer, int size);
    bool saveMarketData(const FutureMarketData* data);
    bool handleMarketData(const FutureMarketData& futureData);
    bool handleTimeSync(const TimeSync& sync);

private:
    tbb::strict_ppl::concurrent_queue<FutureMarketData> m_mktDataQueue;

    unsigned long m_totalMarketDataNum;

    typedef struct {
        string file;
#ifdef _WIN32
        HANDLE hFile;
#else
        FILE* fp;
#endif
        long long lastMotifiedTime; // seconds
    } InstrTargetFile;
    // <filename, target file description>
    tbb::interface5::concurrent_hash_map<string, InstrTargetFile> m_instrFiles;
    string m_dataPath;
    char m_line[2048];
};

#endif // FILE_SYS_DB_H