#ifndef __UAC_H__
#define __UAC_H__

#include "SipServer.h"
#include "InviteSessionHandler.h"

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
    std::shared_ptr<SipServer> m_sipServer;

    std::shared_ptr<CClientRegistrationHandler> m_clientRegisterHandler;
    std::shared_ptr<CInviteSessionHandler> m_inviteHandler;
};




}


#endif