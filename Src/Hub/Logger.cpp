#include <stdarg.h>
#include <sys/stat.h>

#include "MarketHub.h"
#include "Logger.h"

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define PATH_MARK      '\\'
#define MAKE_DIR(a)    _mkdir(a)
#define ACCESS         _access
#else
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#define PATH_MARK      '/'
#define MAKE_DIR(a)    mkdir(a,0777)
#define ACCESS         access
#endif

namespace MarketHub
{

Logger *Logger::m_instance = nullptr;

Logger* Logger::instance()
{
    if (m_instance == nullptr) {
        m_instance = new Logger();
    }

    return m_instance;
}

void Logger::release()
{
    delete m_instance;
    m_instance = nullptr;
}

Logger::Logger()
{
    m_saveLogFile = true;
    m_logFileDir = DEFAULT_LOG_FILE_PATH;
    m_logFile = nullptr;

    if (m_saveLogFile) {
        openLogFile();
    }

    m_queue.clear();
    m_backgroundThread = std::thread(&Logger::backgroundFunction, this);
}

Logger::~Logger()
{
    stop();

    if (m_saveLogFile) {
        closeLogFile();
    }
}

char Logger::getIdentifier(int level)
{
    switch (level) {
    default:
    case LOG_INFO:
        return 'I';

    case LOG_DEBUG:
        return 'D';

    case LOG_WARN:
        return 'W';

    case LOG_ERROR:
        return 'E';

    case LOG_FATAL:
        return 'F';
    }
}

bool Logger::openLogFile()
{
    struct tm tmloc;
    time_t t;
    char fname[1024];
    unsigned long logdate = 0;

    if (ACCESS(m_logFileDir.c_str(), 0) != 0) {
        MAKE_DIR(m_logFileDir.c_str());
    }

    time(&t);
    memcpy(&tmloc, localtime(&t), sizeof(tmloc));
    logdate = ((tmloc.tm_year + 1900) * 100 + tmloc.tm_mon + 1) * 100 + tmloc.tm_mday;
    sprintf(fname, "%s/%ld.log", m_logFileDir.c_str(), logdate);
    
    m_logFile = fopen(fname, "a+");
    
    return (m_logFile != nullptr);
}

void Logger::closeLogFile()
{
    if (m_logFile != nullptr) {
        fflush(m_logFile);
        fclose(m_logFile);
        m_logFile = nullptr;
    }
}

bool Logger::writeLog(int level, const char*log)
{
    if (log == nullptr || log[0] == '\0') {
        return false;
    }

    if (m_saveLogFile) {
        if (m_logFile != nullptr) {
            fprintf(m_logFile, "%s\r\n", log);
            fflush(m_logFile);
            return true;
        }
    }

    return true;
}

bool Logger::writeConsoleLog(int level, const char* log)
{
    std::cout << log << std::endl;
    std::cout.flush();

    return true;
}

void Logger::backgroundFunction()
{
    std::unique_lock<std::mutex> ulock(m_mutex);

    LogData nextLogEntry;
    nextLogEntry.exitFlag = false;

    do {
        if (m_queue.try_pop(nextLogEntry)) {
            if (!nextLogEntry.exitFlag) {
                writeLog(nextLogEntry.level, nextLogEntry.msg);
                writeConsoleLog(nextLogEntry.level, nextLogEntry.msg);
            }
        } else {
            m_condition.wait(ulock);
        }
    } while (!nextLogEntry.exitFlag);
}

void Logger::stop()
{
    LogData logData;
    logData.exitFlag = true;
    m_queue.push(logData);
    m_condition.notify_all();

    m_backgroundThread.join();
}

bool Logger::sendLog(int level, const char* msgFmt, ...)
{
    LogData logData;
    logData.exitFlag = false;
    logData.level = level;

    int bufLen = LOG_MSG_MAX_LENGTH;
    memset(logData.msg, 0, bufLen);

    char timeMsg[32];
#if _WIN32
    SYSTEMTIME lt;
    GetLocalTime(&lt);
    sprintf(timeMsg, "%04d/%02d/%02d %02d:%02d:%02d.%03d",
        lt.wYear,
        lt.wMonth,
        lt.wDay,
        lt.wHour,
        lt.wMinute,
        lt.wSecond,
        lt.wMilliseconds
        );
#else
    struct timeval  tv;
    struct timezone tz;
    struct tm      *tm;
    gettimeofday(&tv, &tz);
    tm = localtime(&tv.tv_sec);
    sprintf(timeMsg, "%04d/%02d/%02d %02d:%02d:%02d.%03d",
        tm->tm_year + 1900,
        tm->tm_mon + 1,
        tm->tm_mday,
        tm->tm_hour,
        tm->tm_min,
        tm->tm_sec,
        tv.tv_usec / 1000);
#endif

    int start = 0;
    int ch = getIdentifier(level);
    start = sprintf(logData.msg, "[%s][%c] ", timeMsg, ch);
    bufLen -= start;

    if (bufLen > 0) {
        va_list args;
        va_start(args, msgFmt);
        int len = vsnprintf(logData.msg + start, bufLen, msgFmt, args);
        va_end(args);

        if (len > 0) {
            m_queue.push(logData);
            m_condition.notify_all();
        }
    }

    return true;
}

} // namespace MarketHub