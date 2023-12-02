#include "SipServer.h"

#include "Common/config.h"

namespace mediakit{


using namespace std;
using namespace std::placeholders;

SipServer::SipServer()
: m_pPollGrp(nullptr)
, m_pEventThreadInterruptor(nullptr)
, m_pSipStack(nullptr)
, m_pDialogUsageManager(nullptr)
, m_pEventStackThread(nullptr)
, m_pDumThread(nullptr)
{

}
SipServer::~SipServer()
{
    clearResources();
}

int SipServer::Initialize()
{
    int iRet = 0;
    __RESIP_TRY__
    // 初始化日志
    resip::Log::initialize("file", "Debug", "gb28181", "./log/resip.log");
    resip::Log::setMaxByteCount(100*1024*1024); //设置单个日志最大长度100M

    if ((iRet = initializeEventPool()) != 0) {
		return iRet;
	}

	if ((iRet = initializeSipStack()) != 0) {
		return iRet;
	}

	if ((iRet = initializeMasterProfile()) != 0) {
		return iRet;
	}	

	if ((iRet = initializeDialogUsageManager()) != 0) {
		return iRet;
	}

	GET_CONFIG(std::string, uaId, Gb28181::kDeviceId);
	GET_CONFIG(std::string, transportType, Gb28181::kSipTransportType);
    GET_CONFIG(uint32_t, localPort, Gb28181::kLocalPort);
    GET_CONFIG(std::string, localIp, Gb28181::kLocalIp);
	GET_CONFIG(std::string, publicIp, Gb28181::kPublicIp);

	InfoL << "SIP server info, id:" << uaId << ", localIp:" << localIp 
				<< ", port:" << localPort << ", type:" << transportType << ", publicIp:" << publicIp;

    // 初始化sip监听
    if (transportType == "TCP") {
		m_pDialogUsageManager->addTransport(resip::TCP, localPort);
	} else if (transportType == "UDP"){
		m_pDialogUsageManager->addTransport(resip::UDP, localPort);
	} else if (transportType == "TLS") {
		m_pDialogUsageManager->addTransport(resip::TLS, localPort);
	} else {
		m_pDialogUsageManager->addTransport(resip::UDP, localPort);
	}

	//设置公共ip/port
	if (publicIp != "")
	{
	//	resip::Data pubip = publicIp.c_str();
	//	m_pDialogUsageManager->setPublicIp(pubip);
	//	m_pDialogUsageManager->setPublicPort(localPort);
	}
	
    resip::Uri fromUri;
	fromUri.user() = uaId.c_str();
	fromUri.host() = localIp.c_str();
	fromUri.port() = localPort;
    m_pMasterProfile->setDefaultFrom(resip::NameAddr(fromUri));

	m_pMasterProfile->setUserAgent(resip::Data("gb28181"));

    __RESIP_CATCH__

    return iRet;
}

int SipServer::Run()
{
    __RESIP_TRY__
    // 启动sip协议栈
    CHECK_POINTER(m_pEventStackThread);
	m_pEventStackThread->run();
	CHECK_POINTER(m_pDumThread);
	m_pDumThread->run();

    __RESIP_CATCH__

    return 0;

}

int SipServer::Shutdown()
{
    CHECK_POINTER(m_pEventStackThread);
	__RESIP_TRY__
	m_pEventStackThread->shutdown();
	m_pEventStackThread->join();
	__RESIP_CATCH__

	CHECK_POINTER(m_pDumThread);  
	__RESIP_TRY__
	m_pDumThread->shutdown();
	m_pDumThread->join();
	__RESIP_CATCH__

    return 0;
}

int SipServer::initializeEventPool()
{
	m_pPollGrp = resip::FdPollGrp::create("epoll");
	CHECK_POINTER(m_pPollGrp);

	m_pEventThreadInterruptor = new resip::EventThreadInterruptor(*m_pPollGrp);
	CHECK_POINTER(m_pEventThreadInterruptor);

	return 0;
}

int SipServer::initializeSipStack()
{
	m_pSipStack = new resip::SipStack(0, resip::DnsStub::EmptyNameserverList, m_pEventThreadInterruptor, false, 0, 0, m_pPollGrp);
	CHECK_POINTER(m_pSipStack);
	m_pSipStack->setFallbackPostNotify(m_pEventThreadInterruptor);
	//Disable Statistics Manager
	m_pSipStack->statisticsManagerEnabled() = false;
	m_pEventStackThread = new resip::EventStackThread(*m_pSipStack, *m_pEventThreadInterruptor, *m_pPollGrp);

	CHECK_POINTER(m_pEventStackThread);

	return 0;	
}

int SipServer::initializeMasterProfile()
{
	std::shared_ptr<resip::MasterProfile> tmp(new resip::MasterProfile);
	m_pMasterProfile = tmp;
	CHECK_POINTER(m_pMasterProfile);

	__RESIP_TRY__
	//support methods
	m_pMasterProfile->clearSupportedMethods();
	m_pMasterProfile->addSupportedMethod(resip::REGISTER);
	m_pMasterProfile->addSupportedMethod(resip::INVITE);
	m_pMasterProfile->addSupportedMethod(resip::MESSAGE);
	m_pMasterProfile->addSupportedMethod(resip::ACK);
	m_pMasterProfile->addSupportedMethod(resip::BYE);
	m_pMasterProfile->addSupportedMethod(resip::CANCEL);
	m_pMasterProfile->addSupportedMethod(resip::INFO);
	m_pMasterProfile->addSupportedMethod(resip::SUBSCRIBE);
	m_pMasterProfile->addSupportedMethod(resip::NOTIFY);

	//support mime
	m_pMasterProfile->clearSupportedMimeTypes();
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("text", "plain"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "KSLP"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "MANSCDP+xml"));
	m_pMasterProfile->addSupportedMimeType(resip::SUBSCRIBE, resip::Mime("application", "MANSCDP+xml"));
	m_pMasterProfile->addSupportedMimeType(resip::NOTIFY, resip::Mime("application", "MANSCDP+xml"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "KSPTZ"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "ALARM"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "KSSP"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "KSDU"));
	m_pMasterProfile->addSupportedMimeType(resip::MESSAGE, resip::Mime("application", "cpim-pidf+xml"));
	m_pMasterProfile->addSupportedMimeType(resip::INVITE, resip::Mime("application", "sdp"));
	m_pMasterProfile->addSupportedMimeType(resip::INVITE, resip::Mime("application", "MANSCDP+xml"));
	m_pMasterProfile->addSupportedMimeType(resip::INFO, resip::Mime("application", "KSPTZ"));
	m_pMasterProfile->addSupportedMimeType(resip::INFO, resip::Mime("application", "KSSP"));
	m_pMasterProfile->addSupportedMimeType(resip::INFO, resip::Mime("application", "MANSRTSP"));
	m_pMasterProfile->addSupportedMimeType(resip::INFO, resip::Mime("application", "rtsp"));
	__RESIP_CATCH__

	return 0;
}

int SipServer::initializeDialogUsageManager()
{
	CHECK_POINTER(m_pSipStack);
	m_pDialogUsageManager = new resip::DialogUsageManager(*m_pSipStack);
	CHECK_POINTER(m_pDialogUsageManager);
	CHECK_POINTER(m_pMasterProfile);
	m_pDialogUsageManager->setMasterProfile(m_pMasterProfile);
	m_pDumThread = new resip::DumThread(*m_pDialogUsageManager);

	CHECK_POINTER(m_pDumThread);

	return 0;		
}

void SipServer::clearResources()
{
	DELETE_POINTER(m_pSipStack);
	DELETE_POINTER(m_pDialogUsageManager);
	DELETE_POINTER(m_pEventStackThread);
	DELETE_POINTER(m_pEventThreadInterruptor);
	DELETE_POINTER(m_pDumThread);
	DELETE_POINTER(m_pPollGrp);
}

//////////////////////////////////////////////////////////////////

RegisterAppDialog::RegisterAppDialog(resip::HandleManager& handleManager,std::string& uasId)
	: resip::AppDialog(handleManager)
	, mUasId(uasId)
{
}

RegisterAppDialogSet::RegisterAppDialogSet(resip::DialogUsageManager& dialogUsageManager, std::string& uasId)
	: resip::AppDialogSet(dialogUsageManager)
	, mUasId(uasId)
{
}


resip::AppDialog* RegisterAppDialogSet::createAppDialog(const resip::SipMessage& message)
{
	return new RegisterAppDialog(mDum, mUasId);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClientRegistrationHandler::CClientRegistrationHandler(resip::DialogUsageManager* dum)
: m_pDum(dum)
{
	m_pDum->setClientRegistrationHandler(this);
}


CClientRegistrationHandler::~CClientRegistrationHandler()
{

}

bool CClientRegistrationHandler::UARegistration(std::string uasId, std::string uasIp, int32_t uasPort)
{
	resip::Uri& fromAddr = m_pDum->getMasterProfile()->getDefaultFrom().uri();

	resip::Uri registrationUri;
	registrationUri.user() = uasId.c_str();
	registrationUri.host() = uasIp.c_str();
	registrationUri.port() = uasPort;

	m_pDum->getMasterProfile()->setDigestCredential(registrationUri.host(), registrationUri.user(), (const char*)"123456");

	std::shared_ptr<resip::SipMessage> message = m_pDum->makeRegistration(resip::NameAddr(registrationUri), new RegisterAppDialogSet(*m_pDum, uasId));

	message->header(resip::h_Vias).front().sentHost() = fromAddr.host();
	message->header(resip::h_Vias).front().sentPort() = fromAddr.port();


	message->header(resip::h_From).uri().user() = fromAddr.user();
	//34020000001320000015 3402000000
	message->header(resip::h_From).uri().host() = fromAddr.host();
	message->header(resip::h_From).uri().port() =  fromAddr.port();
		
	message->header(resip::h_To).uri().user() = fromAddr.user();
	message->header(resip::h_To).uri().host() = fromAddr.host();
	message->header(resip::h_To).uri().port() = fromAddr.port();

	message->header(resip::h_Contacts).front().uri().user() = fromAddr.user();
	message->header(resip::h_Contacts).front().uri().host() = fromAddr.host();
	message->header(resip::h_Contacts).front().uri().port() = fromAddr.port();
		
	message->header(resip::h_Expires).value() = 90;

	m_pDum->send(message);
}

void CClientRegistrationHandler::onSuccess(resip::ClientRegistrationHandle handle, const resip::SipMessage& response)
{
    
}


void CClientRegistrationHandler::onFailure(resip::ClientRegistrationHandle handle, const resip::SipMessage& response)
{
    
}


void CClientRegistrationHandler::onRemoved(resip::ClientRegistrationHandle handle, const resip::SipMessage& response)
{
    
}


int CClientRegistrationHandler::onRequestRetry(resip::ClientRegistrationHandle handle, int retrySeconds, const resip::SipMessage& response)
{
    return 1;
}


void CClientRegistrationHandler::onFlowTerminated(resip::ClientRegistrationHandle handle)
{
   
}



}