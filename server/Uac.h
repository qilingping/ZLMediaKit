#ifndef __UAC_H__
#define __UAC_H__

#include "SipServer.h"
#include "InviteSessionHandler.h"
#include "PagerMessageHandler.h"

#include "Poller/EventPoller.h"
#include "Poller/Timer.h"

namespace mediakit{


class Uac 
{

public:
    Uac();
    virtual ~Uac();

    static Uac* instance() {
        static Uac uac;
        return &uac;
    }

    int32_t start();
    void stop();

private:
    void RegisterCallback(bool result);

private:
    toolkit::EventPoller::Ptr m_poller;
    toolkit::Timer::Ptr m_keepalive;
    toolkit::Timer::Ptr m_register;

    std::shared_ptr<SipServer> m_sipServer;

    std::shared_ptr<CClientRegistrationHandler> m_clientRegisterHandler;
    std::shared_ptr<CInviteSessionHandler> m_inviteHandler;
    std::shared_ptr<CPagerMessageHandler> m_pagerMessageHandler;
};




}


#endif