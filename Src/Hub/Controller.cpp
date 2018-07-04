#include "MarketHub.h"
#include "ControllerImpl.h"

namespace MarketHub
{

Controller* Controller::m_instance = nullptr;

Controller::Controller()
{
    m_implementor = new ControllerImpl();
}

Controller::~Controller()
{
    delete m_implementor;
    m_implementor = nullptr;
}

Controller* Controller::getInstance()
{
    if (m_instance == nullptr) {
        m_instance = new Controller();
    }

    return m_instance;
}

void Controller::subscribeMsg(int msgType, Subscriber* subscriber)
{
    return m_implementor->subscribeMsg(msgType, subscriber);
}

bool Controller::initialize()
{
    return m_implementor->initialize();
}

int Controller::loadAdaptor(const char* library)
{
    return m_implementor->loadAdaptor(library);
}

void Controller::run()
{
    return m_implementor->run();
}

void Controller::stop()
{
    return m_implementor->stop();
}

bool Controller::decrypt(const char* enc, char* plaintext)
{
    if (enc == nullptr || plaintext == nullptr) {
        return false;
    }

    return ControllerImpl::decrypt(enc, PWD_ENCRYPT_KEY, plaintext);
}

void Controller::release()
{
    delete m_instance;
    m_instance = nullptr;
}

} // namespace MarketHub