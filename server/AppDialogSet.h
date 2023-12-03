
#ifndef __C_APPDIALOG_HXX__
#define __C_APPDIALOG_HXX__

#include <memory>

#include "resip/stack/SdpContents.hxx"
#include "resip/stack/PlainContents.hxx"
#include "resip/stack/SipMessage.hxx"
#include "resip/stack/ShutdownMessage.hxx"
#include "resip/stack/SipStack.hxx"
#include "resip/dum/ClientAuthManager.hxx"
#include "resip/dum/ClientInviteSession.hxx"
#include "resip/dum/ClientRegistration.hxx"
#include "resip/dum/DialogUsageManager.hxx"
#include "resip/dum/DumShutdownHandler.hxx"
#include "resip/dum/InviteSessionHandler.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/dum/RegistrationHandler.hxx"
#include "resip/dum/ServerInviteSession.hxx"
#include "resip/dum/ServerOutOfDialogReq.hxx"
#include "resip/dum/OutOfDialogHandler.hxx"
#include "resip/dum/AppDialog.hxx"
#include "resip/dum/AppDialogSet.hxx"
#include "resip/dum/AppDialogSetFactory.hxx"
#include "rutil/Random.hxx"

using namespace resip;

class CAppHandle
{
public:	
	CAppHandle(){}
	virtual ~CAppHandle(){}
	virtual int Response(int errCode, std::string errmsg){return 0;}
};

class CAppDialogSet : public resip::AppDialogSet
{
public:
	CAppDialogSet(DialogUsageManager* dum, std::shared_ptr<CAppHandle> handle)
	: AppDialogSet(*dum)
	, m_pHandle(handle)
	{

	}
	virtual ~CAppDialogSet(){}

	std::shared_ptr<CAppHandle> GetHandle() {
		std::shared_ptr<CAppHandle> handle = m_pHandle.lock();
		return handle;
	}
	
private:
	std::weak_ptr<CAppHandle> m_pHandle;
};


#endif // __C_APPDIALOG_HXX__

