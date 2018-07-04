#ifndef TIMER_MANAGER_H
#define TIMER_MANAGER_H

#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <set>
#include <cstdint>

#ifndef _MSC_VER
#define NOEXCEPT noexcept
#else
#define NOEXCEPT
#endif

typedef struct {
    int timerId;
} TimerContext;

class TimerManager
{
public:
    typedef uint64_t timer_id;
    typedef std::function<void(int, const TimerContext&)> handler_type;

private:
    std::mutex sync;
    typedef std::unique_lock<std::mutex> ScopedLock;

    std::condition_variable wakeUp;

    handler_type handler;

private:
    typedef std::chrono::steady_clock Clock;
    typedef std::chrono::steady_clock::time_point Timestamp;
    typedef std::chrono::milliseconds Duration;

    struct Instance
    {
        Instance(timer_id id = 0)
            : id(id)
            , running(false)
        {
        }

        Instance(timer_id id, Timestamp next, Duration period, const TimerContext& context) NOEXCEPT
            : id(id)
            , next(next)
            , period(period)
            , running(false)
            , context(context)
        {
        }

        Instance(Instance const& r) = delete;

        Instance(Instance&& r) NOEXCEPT
            : id(r.id)
            , next(r.next)
            , period(r.period)
            , running(r.running)
            , context(r.context)
        {
        }

        Instance& operator=(Instance const& r) = delete;

        Instance& operator=(Instance&& r)
        {
            if (this != &r)
            {
                id = r.id;
                next = r.next;
                period = r.period;
                running = r.running;
                context = r.context;
            }
            return *this;
        }

        timer_id id;
        Timestamp next;
        Duration period;
        bool running;

        TimerContext context;
    };

    typedef std::unordered_map<timer_id, Instance> InstanceMap;
    timer_id nextId;
    InstanceMap active;

    // Comparison functor to sort the timer "queue" by Instance::next
    struct NextActiveComparator
    {
        bool operator()(const Instance &a, const Instance &b) const
        {
            return a.next < b.next;
        }
    };
    NextActiveComparator comparator;

    // Queue is a set of references to Instance objects, sorted by next
    typedef std::reference_wrapper<Instance> QueueValue;
    typedef std::multiset<QueueValue, NextActiveComparator> Queue;
    Queue queue;

    // Thread and exit flag
    std::thread worker;
    bool done;
    void threadStart();

public:
    TimerManager();
    ~TimerManager();
    void setHandler(handler_type&& handler);

    timer_id registerTimer(uint64_t when, uint64_t period, const TimerContext& context);

private:
    timer_id registerTimerImpl(Instance&& item);

public:
    bool destroy(timer_id id);

    bool exists(timer_id id);
};

#endif 