#include "EventHub.h"

namespace MarketHub
{

EventHub::EventHub()
    : m_buffer(MAX_SLOT_NUMBER)
    , m_claimStrategy(MAX_SLOT_NUMBER, m_waitStrategy)
    , m_finishedReading(m_waitStrategy)
    , m_running(false)
{
    m_listener = nullptr;
    m_dispatcher = nullptr;
    m_claimStrategy.add_claim_barrier(m_finishedReading);
}

EventHub::~EventHub()
{
}

void EventHub::setEventListener(EventListener* listener)
{
    m_listener = listener;
}

bool EventHub::startDispatchThread()
{
    if (m_running.load()) {
        return true;
    }

    m_dispatcher = new std::thread(std::bind(&EventHub::dispatch, this));
    std::this_thread::yield();

    return true;
}

bool EventHub::isRunning() const
{
    return m_running.load();
}

void EventHub::stop()
{
    if (!m_running.load()) {
        return;
    }

    m_dispatcher->join();
    m_dispatcher = nullptr;
}

bool EventHub::sendEvent(const EventObject* event)
{
    sequence_range range;

    range = sequence_range(m_claimStrategy.claim_one(), 1);

    sequence_t seq = range.first();
    m_buffer[seq] = *event;
    m_claimStrategy.publish(range);

    return true;
}

void EventHub::dispatch()
{
    bool fls = false;
    if (!m_running.compare_exchange_strong(fls, true)) {
        return;
    }

    sequence_t nextToRead = 0;
    bool exit = false;
    while (!exit) {
        sequence_t available = m_claimStrategy.wait_until_published(nextToRead, nextToRead - 1);
        assert(difference(available, nextToRead) >= 0);
        do {
            auto& evt = m_buffer[nextToRead];
            if (!m_listener->onEvent(&evt)) {
                exit = true;
                break;
            }
        } while (nextToRead++ != available);
        m_finishedReading.publish(available);
    }

    m_running.store(false);
}

} // namespace MarketHub