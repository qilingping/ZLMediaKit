#ifndef __SIP_SERVER_H__
#define __SIP_SERVER_H__

#include <memory>

#include "resip/dum/DialogUsageManager.hxx"
#include "resip/dum/DumThread.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/dum/RegistrationHandler.hxx"
#include "resip/dum/PagerMessageHandler.hxx"
#include "resip/dum/AppDialogSet.hxx"
#include "resip/dum/AppDialog.hxx"
#include "resip/stack/SipStack.hxx"
#include "resip/stack/StackThread.hxx"
#include "resip/stack/EventStackThread.hxx"


#include "rutil/FdPoll.hxx"
#include "rutil/dns/AresDns.hxx"
#include "rutil/Log.hxx"
#include "rutil/Logger.hxx"

namespace mediakit{

//check and release
#define CHECK_POINTER(ptr) do{if(ptr == NULL) return -1;}while(0)
#define CHECK_POINTER_WITHOUT_RETURN(ptr) do{if(ptr == NULL) return;}while(0)
#define CHECK_STRING(str) do{if(str.empty()) return -1;}while(0)
#define CHECK_BOOL(b) do{if(!b) return -1;}while(0)
#define DELETE_POINTER(ptr) do{if(ptr) delete ptr;}while(0)

//try - catch
#if defined(CATCH_RESIP_EXCEPTION)
#undef __RESIP_TRY__
#undef __RESIP_CATCH__
#define __RESIP_TRY__ try {
#define __RESIP_CATCH__ \
} catch(resip::BaseException& e) { \
  WarningLog(<< "Catch exception: " << e); \
  return -1; \
}
#else
#undef  __RESIP_TRY__
#undef  __RESIP_CATCH__
#define __RESIP_TRY__
#define __RESIP_CATCH__
#endif

class SipServer
{
public:
    SipServer();
    virtual ~SipServer();

	int Initialize();
	int Run();
	int Shutdown();

	resip::DialogUsageManager* Dum() {return m_pDialogUsageManager;}
	resip::SipStack* SipStack() {return m_pSipStack;}

private:
	int initializeEventPool();
	int initializeEventThreadInterruptor();
	int initializeSipStack();
	int initializeMasterProfile();
	int initializeDialogUsageManager();
	void clearResources();

private:
	resip::FdPollGrp* m_pPollGrp;
	resip::EventThreadInterruptor* m_pEventThreadInterruptor;
	resip::SipStack* m_pSipStack;
	std::shared_ptr<resip::MasterProfile> m_pMasterProfile;
	resip::DialogUsageManager* m_pDialogUsageManager;
	resip::EventStackThread* m_pEventStackThread;
	resip::DumThread* m_pDumThread;
};

class RegisterAppDialog :public resip::AppDialog
{
public:
    RegisterAppDialog(resip::HandleManager& handleManager,std::string& uasId);
	virtual ~RegisterAppDialog() {}
	std::string& uasId() { return mUasId; }
	std::string mUasId;
};


class RegisterAppDialogSet : public resip::AppDialogSet
{
public:
	RegisterAppDialogSet(resip::DialogUsageManager& dialogUsageManager, std::string& uasId);
	virtual ~RegisterAppDialogSet() {}
	virtual resip::AppDialog* createAppDialog(const resip::SipMessage& message);
	std::string mUasId;
};

class CClientRegistrationHandler : public resip::ClientRegistrationHandler
{
public :
	CClientRegistrationHandler(resip::DialogUsageManager* dum);
	~CClientRegistrationHandler();

	bool UARegistration(std::string uasId, std::string uasIp, int32_t uasPort);

	void SetRegisteCallback(std::function<void(bool)> cb) { m_registeCallback = cb; }

public:
	//Called when registration succeeds or each time it is successfully refreshed.
	virtual void onSuccess(resip::ClientRegistrationHandle handle, const resip::SipMessage& response);

	// Called if registration fails
	virtual void onFailure(resip::ClientRegistrationHandle handle, const resip::SipMessage& response);

	virtual void onRemoved(resip::ClientRegistrationHandle handle, const resip::SipMessage& response);

	virtual int onRequestRetry(resip::ClientRegistrationHandle handle, int retrySeconds, const resip::SipMessage& response);

	virtual void onFlowTerminated(resip::ClientRegistrationHandle handle);
private :
	resip::DialogUsageManager* m_pDum;

	std::function<void(bool)> m_registeCallback;
};


// invite



}

#endif //__SIP_SERVER_H__