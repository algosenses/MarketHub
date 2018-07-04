#ifndef MARKET_HUB_RUNTIME_H
#define MARKET_HUB_RUNTIME_H

#include <string>
#include "EventHub.h"
#include "Adaptor.h"
#include "tbb/concurrent_unordered_map.h"

namespace MarketHub
{

class ControllerImpl;

class Runtime : public EventListener, Subscriber
{
public:
    Runtime();
    ~Runtime();

    void setId(int id);
    int  getId() const;
    void setAdaptorObject(Adaptor* adaptor);
    bool initialize(ControllerImpl* controller);
    void start();
    void stop();
    void destroy();

    void subscribeMarketData(int type, const char* instrument);
    void subscribeAllMarketData(int type);
    void sendMarketData(const Message* msg);
    void notifyStatusChanged(int status, const char* extra);
    void notifyMarketData(int type, void* data);
    bool writeLog(int level, const char* log);

private:
    bool onEvent(const void* evt);
    bool dispatchEvent(const Message* pMsg);
    bool sendLogMessage(const char* msgFmt, ...);
    void doCommand(int cmd, int param);
    void doMarketData(const Message* pMsg);
    void doTimeSync(const TimeSync& sync);
	void onMessage(const Message* msg);

private:
    int m_id;
    int m_status;
    bool m_subscribeAllMarketData;
    tbb::concurrent_unordered_map<std::string, int> m_subscriptions;

    ControllerImpl* m_controller;
    Adaptor* m_adaptor;
    EventHub m_evtHub;
};

} // namespace MarketHub

#endif // MARKET_HUB_RUNTIME_H