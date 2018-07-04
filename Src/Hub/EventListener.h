#ifndef EVENT_LISTENER_H
#define EVENT_LISTENER_H

namespace MarketHub
{

class EventListener {
public:
    virtual ~EventListener() { }
    virtual bool onEvent(const void* evt) = 0;        
};

} // namespace MarketHub

#endif // EVENT_LISTENER_H