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

    // 向上级注册
    GET_CONFIG(std::string, uasId, Register::kUasId);
    GET_CONFIG(std::string, uasIp, Register::kUasIp);
    GET_CONFIG(uint32_t, uasPort, Register::kUasPort);

    if (!uasId.empty() && !uasIp.empty()) {
        InfoL << "Register to UasId:" << uasId << ", ip:" << uasIp << ", port:" << uasPort;
        m_clientRegisterHandler = std::make_shared<CClientRegistrationHandler>(dum);
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


}