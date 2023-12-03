#include "Uac.h"

#include "Util/logger.h"
#include "Common/config.h"

#include "resip/dum/DialogUsageManager.hxx"
#include "resip/stack/SipStack.hxx"


namespace mediakit{

Uac::Uac()
{

}


Uac::~Uac()
{

}


int32_t Uac::start()
{
    m_poller = toolkit::EventPollerPool::Instance().getPoller();

    m_sipServer = std::make_shared<SipServer>();
    m_sipServer->Initialize();

    resip::DialogUsageManager* dum = m_sipServer->Dum();
	resip::SipStack* sipStack = m_sipServer->SipStack();
	if (dum == nullptr || sipStack == nullptr) {
		ErrorL << "dum or sipStack is NULL";
		return -1;
	}

    // 注册invite handler, 处理上级的invite请求
    m_inviteHandler = std::make_shared<CInviteSessionHandler>(dum);

    // 注册page message handler，处理上级的message消息，主要是catalog
    m_pagerMessageHandler = std::make_shared<CPagerMessageHandler>(dum);

    // 向上级注册
    GET_CONFIG(std::string, uasId, Register::kUasId);
    GET_CONFIG(std::string, uasIp, Register::kUasIp);
    GET_CONFIG(uint32_t, uasPort, Register::kUasPort);

    if (!uasId.empty() && !uasIp.empty()) {
        InfoL << "Register to UasId:" << uasId << ", ip:" << uasIp << ", port:" << uasPort;
        m_clientRegisterHandler = std::make_shared<CClientRegistrationHandler>(dum);
        m_clientRegisterHandler->SetRegisteCallback(std::bind(&Uac::RegisterCallback, this, std::placeholders::_1));
        m_clientRegisterHandler->UARegistration(uasId, uasIp, uasPort);
    } else {
        WarnL << "Register error UasId:" << uasId << ", ip:" << uasIp << ", port:" << uasPort;
    }

    m_sipServer->Run();
}

void Uac::stop()
{
    if (m_sipServer) {
        m_sipServer->Shutdown();
    }
}

void Uac::RegisterCallback(bool result)
{
    if (result) {
        InfoL << "regiser success, than start keepalive";
        if (m_keepalive == nullptr) {
            m_keepalive = std::make_shared<toolkit::Timer>((float)60, [this]() -> bool {
                if (m_pagerMessageHandler) {
                    GET_CONFIG(std::string, uasId, Register::kUasId);
                    GET_CONFIG(std::string, uasIp, Register::kUasIp);
                    GET_CONFIG(uint32_t, uasPort, Register::kUasPort);
                    m_pagerMessageHandler->SendKeepalive(uasId, uasIp, uasPort);
                }

                return true;
            }, m_poller);
        } 
    } else {
        // delay to re-register
        InfoL << "regiser failed, register after 30s";
        m_keepalive = nullptr;

        m_register = std::make_shared<toolkit::Timer>((float)30, [this]() -> bool {
            if (m_clientRegisterHandler) {
                GET_CONFIG(std::string, uasId, Register::kUasId);
                GET_CONFIG(std::string, uasIp, Register::kUasIp);
                GET_CONFIG(uint32_t, uasPort, Register::kUasPort);
                m_clientRegisterHandler->UARegistration(uasId, uasIp, uasPort);
            }

            return false;
        }, m_poller);
    }
}


}