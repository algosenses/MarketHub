#include <stdarg.h>
#include "Adaptor.h"
#include "Runtime.h"
#include "Logger.h"

namespace MarketHub
{

Adaptor::Adaptor()
{
    
}

void Adaptor::setRuntime(Runtime* runtime) 
{ 
    m_runtime = runtime; 
}

bool Adaptor::subscribeMarketData(int type, const char* instrument)
{
    if (instrument == nullptr || instrument[0] == '\0') {
        return false;
    }

    m_runtime->subscribeMarketData(type, instrument);

    return true;
}

bool Adaptor::subscribeAllMarketData(int type)
{
    m_runtime->subscribeAllMarketData(type);

    return true;
}

bool Adaptor::sendLogMessage(int level, const char* msgFmt, ...)
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
        return m_runtime->writeLog(level, sMsg);
    }

    return false;
}

bool Adaptor::notifyStatusChanged(int status, const char* extra)
{
    m_runtime->notifyStatusChanged(status, extra);

    return true;
}

bool Adaptor::notifyMarketData(int type, void* data)
{
    m_runtime->notifyMarketData(type, data);

    return true;
}

void Adaptor::onCreate() 
{
    m_runtime->notifyStatusChanged(ADAPTOR_LOAD, nullptr);
}

bool Adaptor::onStart() 
{ 
    return true; 
}

bool Adaptor::onMarketData(int type, const void* data)
{
    return true;
}

bool Adaptor::onTimeSync(const TimeSync& sync)
{
    return true;
}

bool Adaptor::onMessage(const Message* msg) 
{ 
    return true; 
}

bool Adaptor::onStop() 
{ 
    return true; 
}

void Adaptor::onDestroy()
{
}

} // namespace MarketHub