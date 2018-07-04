#include "Runtime.h"
#include "ControllerImpl.h"

namespace MarketHub
{

Runtime::Runtime()
{
    m_id = 0;
    m_adaptor = nullptr;
    m_subscribeAllMarketData = false;

    m_evtHub.setEventListener(this);
    m_evtHub.startDispatchThread();
}

Runtime::~Runtime()
{
    Message msg = { 0 };
    msg.header.type = MsgType_Invalid;
    m_evtHub.sendEvent(&msg);
    m_evtHub.stop();

    delete m_adaptor;
    m_adaptor = nullptr;
}

void Runtime::setAdaptorObject(Adaptor* adaptor)
{
    m_adaptor = adaptor;
}

bool Runtime::initialize(ControllerImpl* controller)
{
    m_controller = controller;

    return true;
}

void Runtime::setId(int id)
{
    m_id = id;
}

int Runtime::getId() const
{
    return m_id;
}

void Runtime::start()
{
    sendLogMessage("Start adaptor.");

    Message msg = { 0 };
    msg.header.type = MsgType_AdaptorCommand;
    msg.header.source = 0;
    msg.header.target = m_id;
    msg.body.adaptorCmd.cmd = ADAPTOR_START;

	onMessage(&msg);
}

void Runtime::stop()
{
    sendLogMessage("Stop adaptor.");

    Message msg = { 0 };
    msg.header.type = MsgType_AdaptorCommand;
    msg.header.source = 0;
    msg.header.target = m_id;
    msg.body.adaptorCmd.cmd = ADAPTOR_STOP;

	onMessage(&msg);
}

void Runtime::destroy()
{
    m_adaptor->onDestroy();
}

void Runtime::subscribeMarketData(int type, const char* instrument)
{
    m_subscriptions.insert(std::make_pair(instrument, 0));
	m_controller->subscribeMsg(type, this);
}

void Runtime::subscribeAllMarketData(int type)
{
    m_controller->subscribeMsg(type, this);
    m_subscribeAllMarketData = true;
}

void Runtime::sendMarketData(const Message* msg)
{
    char* instrument = nullptr;
    int type = msg->header.type;
    if (type == MsgType_FutureMarketData) {
        instrument = (char*)(&msg->body.futureData);
    }

    if (instrument == nullptr) {
        return;
    }

    if (m_subscribeAllMarketData) {
		onMessage(msg);
    } else {
        bool found = (m_subscriptions.find(instrument) != m_subscriptions.end());
        if (found) {
			onMessage(msg);
        }
    }
}

void Runtime::onMessage(const Message* msg)
{
    if (!m_evtHub.isRunning()) {
        return;
    }

    m_evtHub.sendEvent(msg);
}

void Runtime::doCommand(int cmd, int param)
{
    if (cmd == ADAPTOR_START) {
        m_adaptor->onStart();
    } else if (cmd == ADAPTOR_STOP) {
        m_adaptor->onStop();
    }
}

void Runtime::doMarketData(const Message* pMsg)
{
    int type = pMsg->header.type;

    const void* data = nullptr;
    switch (type) {
    case MsgType_FutureMarketData:
        data = &(pMsg->body.futureData);
        break;

    case MsgType_StockMarketData:
        data = &(pMsg->body.stockData);
        break;

    case MsgType_OptionMarketData:
        break;

    default:
        break;
    }

    m_adaptor->onMarketData(type, data);
}

void Runtime::doTimeSync(const TimeSync& sync)
{
    m_adaptor->onTimeSync(sync);
}

bool Runtime::onEvent(const void* evt)
{
    return dispatchEvent((const Message*)evt);
}

bool Runtime::dispatchEvent(const Message* pMsg)
{
    int type = pMsg->header.type;
    switch (type) {
    case MsgType_Invalid:
        return false;
        break;

    case MsgType_AdaptorCommand:
        doCommand(pMsg->body.adaptorCmd.cmd, pMsg->body.adaptorCmd.param);
        return true;
        break;

    case MsgType_FutureMarketData:
    case MsgType_StockMarketData:
    case MsgType_OptionMarketData:
        doMarketData(pMsg);
        return true;
        break;

    case MsgType_TimeSync:
        doTimeSync(pMsg->body.timeSync);
        return true;
        break;
    default:
        return true;
    }

    return true;
}

void Runtime::notifyStatusChanged(int status, const char* extra)
{
    m_status = status;

    m_controller->sendAdaptorStatus(m_id, m_adaptor->type(), m_adaptor->getName(), status, extra);

    return;
}

void Runtime::notifyMarketData(int type, void* data)
{
    m_controller->sendMarketData(m_id, type, data);
}

bool Runtime::writeLog(int level, const char* log)
{
    m_controller->sendLogMsg(level, log);

    return true;
}

bool Runtime::sendLogMessage(const char* msgFmt, ...)
{
    return false;
}

} // namespace MarketHub