#ifndef __SIP_SERVER_H__
#define __SIP_SERVER_H__

#include <memory>

#include "resip/dum/DialogUsageManager.hxx"
#include "resip/dum/DumThread.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/stack/SipStack.hxx"
#include "resip/stack/StackThread.hxx"
#include "resip/stack/EventStackThread.hxx"

#include "rutil/FdPoll.hxx"
#include "rutil/dns/AresDns.hxx"

namespace mediakit{

class SipServer
{
public:
    SipServer();
    virtual ~SipServer();

private:
	resip::FdPollGrp* m_pPollGrp;
	resip::EventThreadInterruptor* m_pEventThreadInterruptor;
	resip::SipStack* m_pSipStack;
	std::shared_ptr<resip::MasterProfile> m_pMasterProfile;
	resip::DialogUsageManager* m_pDialogUsageManager;
	resip::EventStackThread* m_pEventStackThread;
	resip::DumThread* m_pDumThread;
};

}

#endif //__SIP_SERVER_H__