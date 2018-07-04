#ifndef MARKET_HUB_LOGGER_H
#define MARKET_HUB_LOGGER_H

#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include "tbb/concurrent_queue.h"

using std::string;

namespace MarketHub
{

#define DEFAULT_LOG_FILE_PATH    "./log"

class Logger
{
public:
    static Logger* instance();
    bool sendLog(int level, const char* msgFmt, ...);
    void release();
    void stop();

private:
    static const int LOG_MSG_MAX_LENGTH = 1024;
    typedef struct {
        bool exitFlag;
        int  level;
        char msg[LOG_MSG_MAX_LENGTH];
    } LogData;

    bool m_saveLogFile;
    string m_logFileDir;
    FILE* m_logFile;

    tbb::strict_ppl::concurrent_queue<LogData>  m_queue;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::thread m_backgroundThread;

    static Logger *m_instance;

    Logger();
    Logger(Logger const&);
    Logger& operator=(Logger const&);
    ~Logger();

    void backgroundFunction();
    void format(int level, const char* log);
    char getIdentifier(int level);

    bool openLogFile();
    void closeLogFile();
    bool writeLog(int level, const char*log);
    bool writeConsoleLog(int level, const char* log);
};

} // namespace MarketHub

#endif // MARKET_HUB_LOGGER_H