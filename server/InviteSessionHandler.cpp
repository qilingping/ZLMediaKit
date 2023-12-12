#include "resip/stack/PlainContents.hxx"
#include "resip/dum/MasterProfile.hxx"
#include "resip/stack/MessageDecorator.hxx"
#include "resip/dum/ServerInviteSession.hxx"
#include "resip/dum/ClientInviteSession.hxx"
#include "rutil/DnsUtil.hxx"
#include "rutil/Data.hxx"

#include "InviteSessionHandler.h"

#include "Common/config.h"


using namespace std::placeholders;

namespace mediakit{

CClientStream::CClientStream(CInviteSessionHandler* handler, std::shared_ptr<PlayParam> param)
    : m_pClentHandler(handler)
    , m_pSdp(nullptr)
    , m_stParam(param)
    , m_poller(nullptr)
    , m_ssrc("123456")
{
    InfoL << "new...";
}

CClientStream::~CClientStream()
{
    InfoL << "delete...";

    if (m_pSdp != nullptr) {
        delete m_pSdp;
        m_pSdp = nullptr;
    }
}

int32_t CClientStream::StartPlay()
{
    pthread_t threadId = pthread_self();
    InfoL << threadId << " | start...";
    if (m_stParam == nullptr) {
        ErrorL << "start play param is nullptr";
        return -1;
    }

    InfoL << "start play streamId:" << m_stParam->strStreamId;

    std::string vhost = "__defaultVhost__";
    std::string app = "live";
    std::string stream = m_stParam->strStreamId;

    // 先从mediasource中获取一把
    auto mediaSource = MediaSource::find(vhost, app, stream, 0);
    if (!mediaSource) {
        ErrorL << "get media source failed, vhost:" << vhost << ", app:" << app << ", stream_id:" << stream;
        return -1;
    }
    
    InfoL << "got media source, vhost:" << vhost << ", app:" << app << ", stream_id:" << stream;

    // 进到这里就是获取到了mediasource

    MediaSourceEvent::SendRtpArgs args;
    if (m_stParam->strSetupWay == "1") {
        // 上级主动的话，本级就是被动
        args.passive = true;
    } else {
        args.passive = false;
    }
    
    args.dst_url = m_stParam->strReceiveIP;
    args.dst_port = m_stParam->uiReceivePort;
    args.ssrc_multi_send = false;
    args.ssrc = m_ssrc;
    if (m_stParam->strSetupWay == "0") {
        args.is_udp = true;
    } else {
        args.is_udp = false;
    }
    args.src_port = 11111;
    args.pt = 96;
    args.use_ps = true;
    args.recv_stream_id = "";
    InfoL << "start send rtp, receive ip:" << args.dst_url << " ,port:" << args.dst_port << "src port:" << args.src_port
            << ", udp:" << args.is_udp << ", passive:" << args.passive;

    mediaSource->getOwnerPoller()->async([=]() mutable {
        pthread_t threadId = pthread_self();
        InfoL << threadId << " | mediaSource start send rtp";
        mediaSource->startSendRtp(args, std::bind(&CClientStream::startSendRtpRes, shared_from_this(), _1, _2));
    });

    return 0;
}

void CClientStream::StopPlay()
{
    pthread_t threadId = pthread_self();
    InfoL << threadId << " | streamId:" << m_stParam->strStreamId;

    std::string vhost = "__defaultVhost__";
    std::string app = "live";
    std::string stream = m_stParam->strStreamId;

    auto mediaSource = MediaSource::find(vhost, app, stream, 0);
    if (mediaSource) {
        mediaSource->getOwnerPoller()->async([=]() mutable {
            InfoL << "stop send rtp";
            mediaSource->stopSendRtp(m_ssrc);
        });
    }
}


void CClientStream::startSendRtpRes(uint16_t localPort, const toolkit::SockException& ex)
{
    std::shared_ptr<PlayResponseParam> res = std::make_shared<PlayResponseParam>();

    pthread_t threadId = pthread_self();
    InfoL << threadId << " |start send rtp res, ex:" << ex;

    if (ex) {
        res->bOk = false;
        res->strStreamId = m_stParam->strStreamId;
        ErrorL << ex;
    } else {
        res->bOk = true;
        res->strStreamId = m_stParam->strStreamId;

        GET_CONFIG(std::string, publicIp, Gb28181::kPublicIp);
        GET_CONFIG(std::string, localIp, Gb28181::kLocalIp);
        if (!publicIp.empty()) {
            res->strSendIP = publicIp;
        } else {
            res->strSendIP = localIp;
        }
        res->uiSendPort = localPort;
        res->strSsrc = "123456";
        if (m_stParam->strSetupWay == "1") {
            // 上级主动的话，本级就是被动
            res->strSetupWay = "2";
        } else if (m_stParam->strSetupWay == "2") {
            res->strSetupWay = "1";
        }
    }

    if (m_pClentHandler) {
        m_pClentHandler->ResponsePlay(shared_from_this(), res);
    } 
}


CInviteSessionHandler::CInviteSessionHandler(resip::DialogUsageManager* dum, toolkit::EventPoller::Ptr poller)
    : m_pDum(dum)
    , m_poller(poller)
{
    m_pDum->setInviteSessionHandler(this);
    m_pDum->getMasterProfile()->addSupportedMethod(resip::INVITE);

    m_pDum->getMasterProfile()->addSupportedMimeType(resip::INVITE, resip::Mime("application", "sdp"));
    m_pDum->getMasterProfile()->addSupportedMimeType(resip::INVITE, resip::Mime("multipart", "mixed"));
    m_pDum->getMasterProfile()->addSupportedMimeType(resip::INVITE, resip::Mime("multipart", "signed"));
    m_pDum->getMasterProfile()->addSupportedMimeType(resip::INVITE, resip::Mime("multipart", "alternative"));
}

CInviteSessionHandler::~CInviteSessionHandler()
{
    
}

void CInviteSessionHandler::RemoveClientStream(std::string& streamId)
{
    InfoL << " streamId:" << streamId;
    if (m_mapClientStream.find(streamId) != m_mapClientStream.end()) {
        m_mapClientStream.erase(streamId);
    }
}

bool CInviteSessionHandler::ResponsePlay(std::shared_ptr<CClientStream> stream, std::shared_ptr<PlayResponseParam> param)
{
    if (stream == nullptr) {
        ErrorL << "client play fail, requestId:" << param->strStreamId;
        return false;
    }

    if (!param->bOk) {
        WarnL << "start live failed, streamId:" << param->strStreamId;
        stream->GetInviteSessionHandle()->reject(500);
        RemoveClientStream(param->strStreamId);
        return true;
    }

    InfoL << "start live success, strSrcIp" << param->strSendIP << ", iSrcPort" << param->uiSendPort;

    resip::SdpContents playsdp;
	playsdp.session().version() = stream->GetSdp()->session().version();
	playsdp.session().name() = stream->GetSdp()->session().name();
	playsdp.session().connection() =  resip::SdpContents::Session::Connection(resip::SdpContents::IP4, param->strSendIP.c_str());
	playsdp.session().origin() = stream->GetSdp()->session().origin();
	playsdp.session().origin().setAddress(param->strSendIP.c_str());
	playsdp.session().media() = stream->GetSdp()->session().media();
	playsdp.session().media().front().clearAttribute("setup");
	playsdp.session().media().front().clearAttribute("recvonly");
    if (param->strSetupWay == "1") {
        playsdp.session().media().front().addAttribute("setup", "active");
    } else if (param->strSetupWay == "2") {
        playsdp.session().media().front().addAttribute("setup", "passive");
    }
	playsdp.session().media().front().addAttribute("sendonly");
	playsdp.session().media().front().setPort(param->uiSendPort);
	playsdp.session().addTime(stream->GetSdp()->session().getTimes().front());
	playsdp.session().addCustomData(resip::Data("y"), resip::Data(param->strSsrc.c_str()));
    stream->GetInviteSessionHandle()->provideAnswer(playsdp);
    stream->GetInviteSessionHandle()->accept();

    return true;
}


int32_t CInviteSessionHandler::playDji(std::string& vhost, std::string& app, std::string& stream)
{
    if (m_player == nullptr) {
        InfoL << "new dji player";

        MediaTuple tuple;
        tuple.vhost = vhost;
        tuple.app = app;
        tuple.stream = stream;

        ProtocolOption option;
        option.enable_audio = false;
        option.auto_close = false;
        option.enable_rtsp = false;
        option.enable_rtmp = true;
        option.enable_mp4 = false;
        option.enable_fmp4 = false;

        m_player = std::make_shared<DjiLivePlayer>(tuple, option);
    }

    uint32_t err = m_player->play();
    if (err != 0) {
        ErrorL << "dji play failed";
        return -1;
    }

    InfoL << "play success...";
    return 0;
}

// 接收上级的invite消息
void CInviteSessionHandler::onNewSession(resip::ServerInviteSessionHandle handle, resip::InviteSession::OfferAnswerType oat, const resip::SipMessage& msg)
{
    pthread_t threadId = pthread_self();
    InfoL <<  threadId <<  " |.....................";
    resip::SdpContents *sdp = dynamic_cast<resip::SdpContents*>(msg.getContents());
    if (sdp == nullptr) {
        WarnL << "sdp is null";
        handle->reject(404);
        return;
    }

    std::string uasId = msg.header(resip::h_From).uri().user().c_str();
    std::string deviceId = sdp->session().origin().user().c_str();
    std::string mediaIp = sdp->session().connection().getAddress().c_str();
    int mediaPort = sdp->session().media().front().port();
    std::string transport = sdp->session().media().front().protocol().c_str();
	std::string streamtype = sdp->session().media().front().codecs().front().getName().c_str();

    if(transport.find("TCP") == std::string::npos) {
        transport = "0";
    } else {
        transport = "2";
    }

    if (m_mapClientStream.find(deviceId) != m_mapClientStream.end()) {
        WarnL << "stream exist, streamId:" << deviceId;
        handle->reject(400);
        return;
    }
        
    std::string strCallId;
	if (msg.exists(resip::h_CallId)) {
		strCallId = std::string(msg.header(resip::h_CallId).value().c_str());
	} else {
        WarnL << "not exist callid";
		handle->reject(400);
		return;
	}

    // 回复100trying
    handle->provisional(100);

    InfoL << "new invite, streamId:" << deviceId;

    // 先保证dji player成功
    std::string vhost = "__defaultVhost__";
    std::string app = "live";
    std::string stream = deviceId; 
    playDji(vhost, app, stream);

    std::shared_ptr<PlayParam> param = std::make_shared<PlayParam>();
    param->strPlayType = "live";
    param->strRequestId = strCallId;
    param->strStreamId = deviceId;
    param->strReceiveIP = mediaIp;
    param->uiReceivePort = mediaPort;
    param->strStreamType = "MAIN";
    param->vedioEncodingType = "PS";     // 码流编码类型,"1":PS, "2":H.264, "3": SVAC, "4":H.265
    param->strSetupWay = transport;           // "0"-udp,"1"-tcp主动,2-tcp被动。passive or active 仅TCP模式下
    param->audioEncodingType = "1";     //音频编码型，"1":G711A,"2":G711U,"3":SVAC,"4"G723.1:,"5":G729,"6":G722.1
    param->audioSampleRate = "1";       //音频采样率"1".8K "2".14K "3".16K "4".32K
    param->iTimeout = 5;                       //超时时间

    std::shared_ptr<CClientStream> client = std::make_shared<CClientStream>(this, param);
    resip::SdpContents *pUasSdp = dynamic_cast<resip::SdpContents*>(sdp->clone());
    client->SetSdp(pUasSdp);
    client->SetServerInviteSessionHandle(handle);

    m_mapClientStream[param->strStreamId] = client;

    int32_t err = client->StartPlay();
    if (err != 0) {
        // play failed
        ErrorL <<  threadId <<  " | play failed";
        handle->reject(500);
        RemoveClientStream(param->strStreamId);
    }  
}

void CInviteSessionHandler::onConnectedConfirmed(resip::InviteSessionHandle handle, const resip::SipMessage &msg)
{
    std::string streamId;
    if (msg.exists(resip::h_To)) {
        streamId = msg.header(resip::h_To).uri().user().c_str();
    }

    InfoL << "client play connect success, streamId:" << streamId;
}

void CInviteSessionHandler::onAnswer(resip::InviteSessionHandle handle, const resip::SipMessage& msg, const resip::SdpContents& sdp)
{
    std::string streamId;
    if (msg.exists(resip::h_To)) {
        streamId = msg.header(resip::h_To).uri().user().c_str();
    }

    InfoL << "client play ack success, streamId:" << streamId;
}


void CInviteSessionHandler::onTerminated(resip::InviteSessionHandle handle, resip::InviteSessionHandler::TerminatedReason reason, const resip::SipMessage* message)
{
/*
enum TerminatedReason
	 {
	    Error, 
	    Timeout,
	    Replaced,
	    LocalBye, 
	    RemoteBye
	 };
*/

    pthread_t threadId = pthread_self();
    InfoL << threadId << " | onTerminated, reason = " << reason;
    // handle->reject会在这里收到terminated
    if (message == nullptr) {
        WarnL << "message is nullptr";
        return;
    }

    // 下级收到上级下发的bye
    std::string deviceId, uasid, callid;
    deviceId = message->header(resip::h_To).uri().user().c_str();
    uasid = message->header(resip::h_From).uri().user().c_str();
    callid = message->header(resip::h_CallId).value().c_str();

    std::shared_ptr<CClientStream> stream;
    if (m_mapClientStream.find(deviceId) != m_mapClientStream.end()) {
        stream = m_mapClientStream[deviceId];
    }

    if (stream == nullptr) {
        ErrorL << "onTerminated, stream not exist, streamId:" << deviceId;
        return;
    }
    
    InfoL << "onTerminated streamId:" << deviceId;
    stream->StopPlay();

    RemoveClientStream(deviceId);
}

/// called when MESSAGE message is received 
void CInviteSessionHandler::onMessage(resip::InviteSessionHandle handle, const resip::SipMessage& msg)
{
    
}

}



