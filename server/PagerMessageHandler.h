
#ifndef __PAGERMESSAGEHANDLER_H__
#define __PAGERMESSAGEHANDLER_H__

#include "resip/dum/PagerMessageHandler.hxx"
#include "resip/dum/DialogUsageManager.hxx"

#include "resip/dum/AppDialog.hxx"
#include "resip/dum/AppDialogSet.hxx"

#include <atomic> 


namespace mediakit{

class PageAppDialogSet : public resip::AppDialogSet
{
public:
	PageAppDialogSet(resip::DialogUsageManager& dialogUsageManager)
		: resip::AppDialogSet(dialogUsageManager) {}
	virtual ~PageAppDialogSet() {}
	virtual resip::AppDialog* createAppDialog(const resip::SipMessage& message) { new resip::AppDialog(mDum); }
};

class CPagerMessageHandler : public resip::ClientPagerMessageHandler, public resip::ServerPagerMessageHandler
{
public:
	CPagerMessageHandler(resip::DialogUsageManager* dum);
	virtual ~CPagerMessageHandler();


	bool SendKeepalive(std::string& uasId, std::string& uasIp, int port);
	bool SendCatalogList(std::string deviceId, std::string fromIp, int32_t fromPort, uint64_t sn);

public:

	virtual void onSuccess(resip::ClientPagerMessageHandle, const resip::SipMessage& status) {}
      //!kh!
      // Application could re-page the failed contents or just ingore it.
    virtual void onFailure(resip::ClientPagerMessageHandle, const resip::SipMessage& status, std::unique_ptr<resip::Contents> contents) {}

	virtual void onMessageArrived(resip::ServerPagerMessageHandle handle, const resip::SipMessage& message);

	int processMessage(std::string& contents, const resip::SipMessage& message);

private:
	resip::DialogUsageManager* m_pDum;
	std::atomic<uint64_t> m_ulSN;
};

}	// namespace mediakit

#endif	// __GB28181_PAGERMESSAGEHANDLER_H__
