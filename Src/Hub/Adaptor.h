#ifndef MARKET_HUB_ADAPTOR_H
#define MARKET_HUB_ADAPTOR_H

#include <mutex>
#include "MarketHub.h"

#ifdef ADAPTOR_EXPORTS
    #define ADAPTOR_API __declspec(dllexport)
#else
    #define ADAPTOR_API __declspec(dllimport)
#endif

namespace MarketHub
{

class Runtime;

class DllExport Adaptor
{
public:
    enum AdaptorType {
        SOURCE,
        SINK,
    };

    Adaptor();
    virtual ~Adaptor() {}
    void setRuntime(Runtime* runtime);
    virtual int         type() const = 0;
    virtual const char* getName() const = 0;
    virtual void onCreate();
    virtual bool onStart();
    virtual bool onMarketData(int type, const void* data);
    virtual bool onTimeSync(const TimeSync& sync);
    virtual bool onMessage(const Message* msg);
    virtual bool onStop();
    virtual void onDestroy();

	virtual bool retrieveData() { return false; }
	virtual bool storeData() { return false; }

protected:
    int  getId() const;
    bool subscribeMarketData(int type, const char* instrument);
    bool subscribeAllMarketData(int type);
    bool sendLogMessage(int level, const char* msgFmt, ...);
    bool notifyStatusChanged(int status, const char* extra);
    bool notifyMarketData(int type, void* data);

private:
    Runtime* m_runtime;
};

extern "C" typedef Adaptor *CreateAdaptorProc();

#define EXPORT_ADAPTOR(adaptor_name)                             \
    extern "C" ADAPTOR_API Adaptor* CreateAdaptor()              \
    {                                                            \
        return new adaptor_name;                                 \
    }

} // namespace MarketHub

#endif // MARKET_HUB_ADAPTOR_H