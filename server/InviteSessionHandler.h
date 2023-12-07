#ifndef __INVITE_SESSION_HANDLER_H__
#define __INVITE_SESSION_HANDLER_H__

#include <unordered_map>
#include <memory>

#include "resip/dum/InviteSessionHandler.hxx"
#include "resip/dum/DialogUsageManager.hxx"
#include "resip/dum/AppDialogSet.hxx"

#include "Util/logger.h"
#include "Player/PlayerProxy.h"
#include "Common/MediaSource.h"

#include "Poller/EventPoller.h"

namespace mediakit{

typedef struct PlayParam
{
	std::string strPlayType;
	std::string strRequestId;
	std::string strStreamId;
	std::string strReceiveIP;			// 本级收流端IP
	unsigned int uiReceivePort;			// 本级收流端端口
	std::string strStreamType;			//主子码流，"MAIN":主码流，"SUB":子码流, 目前只有与大华定制化接口这样传值, 为空则此字段无效
	std::string vedioEncodingType;     // 码流编码类型,"1":PS, "2":H.264, "3": SVAC, "4":H.265
	std::string strSetupWay;           // "0"-udp,"1"-tcp主动,2-tcp被动。passive or active 仅TCP模式下
	std::string audioEncodingType;     //音频编码型，"1":G711A,"2":G711U,"3":SVAC,"4"G723.1:,"5":G729,"6":G722.1
	std::string audioSampleRate;       //音频采样率"1".8K "2".14K "3".16K "4".32K
	std::string strPushUrl;
	int iTimeout;                       //超时时间
}PlayParam;

typedef struct PlayResponseParam
{
	bool bOk;
	std::string strRequestId;
	std::string strSendIP;			// 本级收流端IP
	unsigned int uiSendPort;			// 本级收流端端口
	std::string strSsrc;
	std::string strSetupWay;           // "0"-udp,"1"-tcp主动,2-tcp被动。passive or active 仅TCP模式下
}PlayResponseParam;

class CInviteSessionHandler;
class CClientStream	: public std::enable_shared_from_this<CClientStream>
{
public:
	CClientStream(CInviteSessionHandler* handler, std::shared_ptr<PlayParam> param, toolkit::EventPoller::Ptr poller);
	~CClientStream();

	void SetSdp(resip::SdpContents* sdp) {m_pSdp = sdp;}
	void SetServerInviteSessionHandle(resip::ServerInviteSessionHandle handle) { m_pHandle = handle; } 

	int32_t StartPlay();
	void StopPlay();
	
	resip::SdpContents* GetSdp() {return m_pSdp;}
	resip::ServerInviteSessionHandle GetInviteSessionHandle() {return m_pHandle;}

private:
	void startSendRtpRes(uint16_t localPort, const toolkit::SockException& ex);

private:
	CInviteSessionHandler* m_pClentHandler;
	resip::ServerInviteSessionHandle m_pHandle;
	resip::SdpContents* m_pSdp;
	std::shared_ptr<PlayParam> m_stParam;

	// player执行线程
	toolkit::EventPoller::Ptr m_poller;

private:	// stream 相关
	std::shared_ptr<PlayerProxy> m_player;
	MediaSource::Ptr m_mediaSource;

	std::string m_ssrc;
};


class CInviteSessionHandler : public resip::InviteSessionHandler
{
public:
	CInviteSessionHandler(resip::DialogUsageManager* dum, toolkit::EventPoller::Ptr poller);
	~CInviteSessionHandler();

public:

	bool ResponsePlay(std::shared_ptr<CClientStream> stream, std::shared_ptr<PlayResponseParam> param);
	void RemoveClientStream(std::string& streamId);

private:
	resip::DialogUsageManager* m_pDum;
	toolkit::EventPoller::Ptr m_poller;

	std::mutex m_muxClientStream;
	std::unordered_map<std::string, std::shared_ptr<CClientStream> > m_mapClientStream;

public:		// 继承自InviteSessionHandler
	/// called when an initial INVITE or the intial response to an outoing invite  
	virtual void onNewSession(resip::ClientInviteSessionHandle handle, resip::InviteSession::OfferAnswerType oat, const resip::SipMessage& msg) {}
	virtual void onNewSession(resip::ServerInviteSessionHandle handle, resip::InviteSession::OfferAnswerType oat, const resip::SipMessage& msg);

	/// Received a failure response from UAS
	virtual void onFailure(resip::ClientInviteSessionHandle handle, const resip::SipMessage& msg) {}

	/// called when an in-dialog provisional response is received that contains a body
	virtual void onEarlyMedia(resip::ClientInviteSessionHandle, const resip::SipMessage&, const resip::SdpContents&){}
	virtual void onEarlyMedia(resip::ClientInviteSessionHandle, const resip::SipMessage&, const resip::Contents&){}

	/// called when dialog enters the Early state - typically after getting 18x
	virtual void onProvisional(resip::ClientInviteSessionHandle, const resip::SipMessage&){}

	/// called when a dialog initiated as a UAC enters the connected state
	virtual void onConnected(resip::ClientInviteSessionHandle handle, const resip::SipMessage& msg) {}

	/// called when a dialog initiated as a UAS enters the connected state
	virtual void onConnected(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when ACK (with out an answer) is received for initial invite (UAS)
	virtual void onConnectedConfirmed(resip::InviteSessionHandle, const resip::SipMessage &msg);

	/// called when PRACK is received for a reliable provisional answer (UAS)
	virtual void onPrack(resip::ServerInviteSessionHandle, const resip::SipMessage &msg){}

	/** UAC gets no final response within the stale call timeout (default is 3
	 * minutes). This is just a notification. After the notification is
	 * called, the InviteSession will then call
	 * InviteSessionHandler::terminate() */
	virtual void onStaleCallTimeout(resip::ClientInviteSessionHandle h){}

	/** called when an early dialog decides it wants to terminate the
	 * dialog. Default behavior is to CANCEL all related early dialogs as
	 * well.  */
	virtual void terminate(resip::ClientInviteSessionHandle h){}


	virtual void onTerminated(resip::InviteSessionHandle, resip::InviteSessionHandler::TerminatedReason reason, const resip::SipMessage* related=0);

	/// called when a fork that was created through a 1xx never receives a 2xx
	/// because another fork answered and this fork was canceled by a proxy. 
	virtual void onForkDestroyed(resip::ClientInviteSessionHandle) {}

	/// called when a 3xx with valid targets is encountered in an early dialog     
	/// This is different then getting a 3xx in onTerminated, as another
	/// request will be attempted, so the DialogSet will not be destroyed.
	/// Basically an onTermintated that conveys more information.
	/// checking for 3xx respones in onTerminated will not work as there may
	/// be no valid targets.
	virtual void onRedirected(resip::ClientInviteSessionHandle, const resip::SipMessage& msg){}

	/// called to allow app to adorn a message. default is to send immediately
	virtual void onReadyToSend(resip::InviteSessionHandle, resip::SipMessage& msg){}

	/// called when an answer is received - has nothing to do with user
	/// answering the call 
	virtual void onAnswer(resip::InviteSessionHandle handle, const resip::SipMessage& msg, const resip::SdpContents& sdp);
	// You should only override the following method if genericOfferAnswer is true
//	virtual void onAnswer(InviteSessionHandle handle, const SipMessage& msg, const Contents& sdp){}

	/// called when an offer is received - must send an answer soon after this
	virtual void onOffer(resip::InviteSessionHandle, const resip::SipMessage& msg, const resip::SdpContents&){}      
	// You should only override the following method if genericOfferAnswer is true
	virtual void onOffer(resip::InviteSessionHandle, const resip::SipMessage& msg, const resip::Contents&){}      

	/// called when a modified body is received in a 2xx response to a
	/// session-timer reINVITE. Under normal circumstances where the response
	/// body is unchanged from current remote body no handler is called
	virtual void onRemoteSdpChanged(resip::InviteSessionHandle, const resip::SipMessage& msg, const resip::SdpContents&){}
	// You should only override the following method if genericOfferAnswer is true
	virtual void onRemoteAnswerChanged(resip::InviteSessionHandle, const resip::SipMessage& msg, const resip::Contents&){} 

	/// Called when an error response is received for a reinvite-nobody request (via requestOffer)
	virtual void onOfferRequestRejected(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when an Invite w/out offer is sent, or any other context which
	/// requires an offer from the user
	virtual void onOfferRequired(resip::InviteSessionHandle, const resip::SipMessage& msg){}      

	/// called if an offer in a UPDATE or re-INVITE was rejected - not real
	/// useful. A SipMessage is provided if one is available
	virtual void onOfferRejected(resip::InviteSessionHandle, const resip::SipMessage* msg){}

	/// called when INFO message is received 
	/// the application must call acceptNIT() or rejectNIT()
	/// once it is ready for another message.
	virtual void onInfo(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when response to INFO message is received 
	virtual void onInfoSuccess(resip::InviteSessionHandle, const resip::SipMessage& msg){}
	virtual void onInfoFailure(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when MESSAGE message is received 
	virtual void onMessage(resip::InviteSessionHandle, const resip::SipMessage& msg);

	/// called when response to MESSAGE message is received 
	virtual void onMessageSuccess(resip::InviteSessionHandle, const resip::SipMessage& msg){}
	virtual void onMessageFailure(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when an REFER message is received.  The refer is accepted or
	/// rejected using the server subscription. If the offer is accepted,
	/// DialogUsageManager::makeInviteSessionFromRefer can be used to create an
	/// InviteSession that will send notify messages using the ServerSubscription
	virtual void onRefer(resip::InviteSessionHandle, resip::ServerSubscriptionHandle, const resip::SipMessage& msg){}

	virtual void onReferNoSub(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when an REFER message receives a failure response 
	virtual void onReferRejected(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// called when an REFER message receives an accepted response 
	virtual void onReferAccepted(resip::InviteSessionHandle, resip::ClientSubscriptionHandle, const resip::SipMessage& msg){}

	/// called when ACK is received
	virtual void onAckReceived(resip::InviteSessionHandle, const resip::SipMessage& msg){}

	/// default behaviour is to send a BYE to end the dialog
	virtual void onAckNotReceived(resip::InviteSessionHandle){}

	/// UAC gets no final response within the stale re-invite timeout (default is 40
	/// seconds).  Default behaviour is to send a BYE to end the dialog.
	virtual void onStaleReInviteTimeout(resip::InviteSessionHandle h){}

	/// will be called if reINVITE or UPDATE in dialog fails
	virtual void onIllegalNegotiation(resip::InviteSessionHandle h, const resip::SipMessage& msg){}     

	/// will be called if Session-Timers are used and Session Timer expires
	/// default behaviour is to send a BYE to send the dialog
	virtual void onSessionExpired(resip::InviteSessionHandle){}

	/// Called when a TCP or TLS flow to the server has terminated.  This can be caused by socket
	/// errors, or missing CRLF keep alives pong responses from the server.
	//  Called only if clientOutbound is enabled on the UserProfile and the first hop server 
	/// supports RFC5626 (outbound).
	/// Default implementation is to do nothing
	virtual void onFlowTerminated(resip::InviteSessionHandle){}
};

}

#endif //__INVITE_SESSION_HANDLER_H__ 
