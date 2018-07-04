#ifndef EVENT_HUB_H
#define EVENT_HUB_H

#include <thread>
#include "ring_buffer.hpp"
#include "single_threaded_claim_strategy.hpp"
#include "multi_threaded_claim_strategy.hpp"
#include "blocking_wait_strategy.hpp"
#include "spin_wait_strategy.hpp"
#include "sequence_barrier.hpp"

#include "MarketHub.h"
#include "EventListener.h"

using namespace disruptorplus;

namespace MarketHub
{

typedef Message EventObject;

#define MAX_SLOT_NUMBER          (8192)

class EventHub
{
public:
    EventHub();
    ~EventHub();
    void setEventListener(EventListener* listener);
    bool startDispatchThread();
    bool sendEvent(const EventObject* event);
    bool isRunning() const;
    void stop();

private:
    void dispatch();

private:
    std::atomic<bool> m_running;
    EventListener* m_listener;
    std::thread*   m_dispatcher;

    ring_buffer<EventObject> m_buffer;
    blocking_wait_strategy m_waitStrategy;
    multi_threaded_claim_strategy<blocking_wait_strategy> m_claimStrategy;
    sequence_barrier<blocking_wait_strategy> m_finishedReading;
};

} // namespace MarketHub

#endif // EVENT_HUB_H