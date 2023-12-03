
#include "resip/stack/PlainContents.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/dum/ClientPagerMessage.hxx"
#include "resip/dum/ServerPagerMessage.hxx"
#include "resip/dum/AppDialogSet.hxx"

#include "PagerMessageHandler.h"
#include "XmlHelper.h"

#include "Util/logger.h"
#include "Common/config.h"

#include <memory>

namespace mediakit{

CPagerMessageHandler::CPagerMessageHandler(resip::DialogUsageManager* dum, toolkit::EventPoller::Ptr poller)
	: m_pDum(dum)
	, m_poller(poller)
{
	m_ulSN.store(1);

	m_pDum->getMasterProfile()->addSupportedMethod(resip::ACK);
	m_pDum->getMasterProfile()->addSupportedMethod(resip::CANCEL);
	m_pDum->getMasterProfile()->addSupportedMethod(resip::OPTIONS);
	m_pDum->getMasterProfile()->addSupportedMethod(resip::MESSAGE);
	m_pDum->getMasterProfile()->addSupportedMethod(resip::SUBSCRIBE);

	m_pDum->getMasterProfile()->addSupportedMimeType(resip::OPTIONS, resip::Mime("application", "sdp"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::OPTIONS, resip::Mime("multipart", "mixed"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::OPTIONS, resip::Mime("multipart", "signed"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::OPTIONS, resip::Mime("multipart", "alternative"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::ACK, resip::Mime("Application", "MANSCDP+xml"));

	m_pDum->getMasterProfile()->addSupportedMimeType(resip::MESSAGE, resip::Mime("text", "plain"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::MESSAGE, resip::Mime("Application", "MANSCDP+xml"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::MESSAGE, resip::Mime("Application", "xml"));		

	m_pDum->getMasterProfile()->addSupportedMimeType(resip::SUBSCRIBE, resip::Mime("text", "plain"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::SUBSCRIBE, resip::Mime("Application", "MANSCDP+xml"));
	m_pDum->getMasterProfile()->addSupportedMimeType(resip::SUBSCRIBE, resip::Mime("Application", "xml"));

	m_pDum->setServerPagerMessageHandler(this);
	m_pDum->setClientPagerMessageHandler(this);
}

CPagerMessageHandler::~CPagerMessageHandler()
{
}

bool CPagerMessageHandler::SendKeepalive(std::string& uasId, std::string& uasIp, int port)
{
	resip::NameAddr toAddr;
	toAddr.uri().host() = uasIp.c_str();
	toAddr.uri().port() = port;
	toAddr.uri().user() = uasId.c_str();

	uint64_t sn = m_ulSN.fetch_add(1);
	CXmlHelper xmlHelper;
	if (!xmlHelper.buildKeepaliveXml(uasId, sn))
	{
		ErrorL << "buildKeepaliveXml error when SendKeepalive";
		return false;
	}
	std::string strContents = xmlHelper.xmlMessage();	

	resip::ClientPagerMessageHandle pHandle = m_pDum->makePagerMessage(toAddr, new PageAppDialogSet(*m_pDum));
	std::unique_ptr<resip::PlainContents> contents(new resip::PlainContents(strContents.c_str(), resip::Mime("Application", "MANSCDP+xml")));
	if (pHandle.get() != nullptr) {
		pHandle.get()->page(std::move(contents));
	} else {
		WarnL << "pagerMessage handle is null";
	}
	

	InfoL << "send keepalive to uas";

	return true;
}

bool CPagerMessageHandler::SendCatalogList(std::string deviceId, std::string fromIp, int32_t fromPort, uint64_t sn)
{
	resip::NameAddr toAddr;
	toAddr.uri().host() = fromIp.c_str();
	toAddr.uri().port() = fromPort;
	toAddr.uri().user() = deviceId.c_str();

	GET_CONFIG(std::string, uaId, Gb28181::kDeviceId);
	GET_CONFIG(std::string, channelId, Gb28181::kChannelId);

	CXmlHelper xmlHelper;
	if (!xmlHelper.buildCatalogResponseXml(channelId, uaId, sn))
	{
		ErrorL << "buildCatalogResponseXml error when SendKeepalive";
		return false;
	}
	std::string strContents = xmlHelper.xmlMessage();	

	InfoL << "SendCatalogList:port:" << toAddr.uri().port() << ", id:" << toAddr.uri().user();
	InfoL << "SendCatalogList message:" << strContents;

	resip::ClientPagerMessageHandle pHandle = m_pDum->makePagerMessage(toAddr, new PageAppDialogSet(*m_pDum));
	std::unique_ptr<resip::PlainContents> contents(new resip::PlainContents(strContents.c_str(), resip::Mime("Application", "MANSCDP+xml")));
	if (pHandle.get() != nullptr) {
		pHandle.get()->page(std::move(contents));
	} else {
		WarnL << "pagerMessage handle is null";
	}

	return true;
}

void CPagerMessageHandler::onMessageArrived(resip::ServerPagerMessageHandle handle, const resip::SipMessage& message)
{
	int retCode = 0;
	resip::Contents* contentBody = message.getContents();
	if (NULL == contentBody || contentBody->getBodyData().empty()){
		// Maybe some message has no body data.
		handle->send(handle->accept(400));
		return;
	}

	std::string content;
	retCode = processMessage(content, message);
	if (retCode == 0){
		handle->send(handle->accept(200));
	} else{
		handle->send(handle->accept(retCode));
	}
}

int CPagerMessageHandler::processMessage(std::string& contents, const resip::SipMessage& message)
{
	int ierrcode = 400;
	std::string strerrcode, strerrmsg; 
	CXmlHelper::MessageHeader msgHeader;
	CXmlHelper xmlHelper(message.getContents()->getBodyData().c_str());

	// 先尝试按国标的方式解析，再按照db33方式解析
	if (!xmlHelper.analyzeHeader(msgHeader)) {
		return ierrcode;
	}

	std::string fromUser = message.header(resip::h_From).uri().user().c_str();
	std::string fromIp = message.header(resip::h_From).uri().host().c_str();
	int fromPort = message.header(resip::h_From).uri().port();

	/*国标消息, 目前仅仅支持catalog*/
	if (msgHeader.strMethod == "Query"){
		if(msgHeader.strCmdType == "LoadStatus"){

		} else if (msgHeader.strCmdType == "Catalog") {
			InfoL << "catalog deviceId:" << msgHeader.strDeviceID;
			SendCatalogList(fromUser, fromIp, fromPort, msgHeader.ulSN);
			return 0;
		}
    } else {
		
	}
	
	return ierrcode;
}

}