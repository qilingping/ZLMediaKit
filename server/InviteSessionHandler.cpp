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

CClientStream::CClientStream(CInviteSessionHandler* handler, std::shared_ptr<PlayParam> param, toolkit::EventPoller::Ptr poller)
    : m_pClentHandler(handler)
    , m_stParam(param)
    , m_poller(poller)
    , m_ssrc("123456")
{
    InfoL << "new...";
}

CClientStream::~CClientStream()
{
    InfoL << "delete...";
}

int32_t CClientStream::StartPlay()
{
    std::string vhost = "__defaultVhost__";
    std::string app = "live";
    std::string stream = m_stParam->strStreamId;
    std::string url = "dji://liveview";
    int32_t retry_count = 0;    // 不重试
    ProtocolOption option;
    option.enable_audio = false;
    option.auto_close = false;
    option.enable_rtmp = true;

    m_player = std::make_shared<PlayerProxy>(vhost, app, stream, option, retry_count);

    //开始播放，如果播放失败或者播放中止，将会自动重试若干次，默认一直重试
    m_player->setPlayCallbackOnce([=, this](const toolkit::SockException &ex) {
        if (ex) {
            ErrorL << "play dji stream failed, ex:" << ex;
            std::shared_ptr<PlayResponseParam> res = std::make_shared<PlayResponseParam>();
            res->bOk = false;
            if (m_pClentHandler) {
                m_pClentHandler->ResponsePlay(shared_from_this(), res);
            } 
        } else {
            InfoL << "play dji live stream success";

            // 开启推流到gb上级
            auto mediaSource = MediaSource::find(vhost, app, stream, 0);
            if (!mediaSource) {
                ErrorL << "can not find the source stream, vhost:" << vhost << ", app:" << app << ", stream_id:" << stream;
                return;
            }

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
            TraceL << "startSendRtp, pt " << int(args.pt) << " ps " << args.use_ps << " audio " << args.only_audio;

            mediaSource->getOwnerPoller()->async([=]() mutable {
                mediaSource->startSendRtp(args, std::bind(&CClientStream::startSendRtpRes, shared_from_this(), _1, _2));
            });
        }
    });

    //被主动关闭拉流
    m_player->setOnClose([this](const toolkit::SockException &ex) {
    //    StopPlay();
    });

    InfoL << "start send play func";
    m_poller->async([this, url]() {
        m_player->play(url);
    });
}

void CClientStream::StopPlay()
{
    m_poller->async([this]() {
        std::string vhost = "__defaultVhost__";
        std::string app = "live";
        std::string stream = m_stParam->strStreamId;

        auto mediaSource = MediaSource::find(vhost, app, stream, 0);
        if (mediaSource) {
        //    InfoL << "media source closing";
        //    mediaSource->close(true);
        //    InfoL << "media source closed";
        //    mediaSource->unregist();
        //    InfoL << "media source unregist";
        //    m_mediaSource->stopSendRtp(m_ssrc);

        //    InfoL << "stop send rtp";
        }

        if (m_pClentHandler) {
            m_pClentHandler->RemoveClientStream(m_stParam->strStreamId);
        } 
    });
    
}

void CClientStream::startSendRtpRes(uint16_t localPort, const toolkit::SockException& ex)
{
    std::shared_ptr<PlayResponseParam> res = std::make_shared<PlayResponseParam>();

    if (ex) {
        res->bOk = false;
        ErrorL << ex;
    } else {
        res->bOk = true;

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
    std::lock_guard<std::mutex> g(m_muxClientStream);
    m_mapClientStream.clear();
}

void CInviteSessionHandler::RemoveClientStream(std::string& streamId)
{
    std::lock_guard<std::mutex> g(m_muxClientStream);
    if (m_mapClientStream.find(streamId) != m_mapClientStream.end()) {
        m_mapClientStream.erase(streamId);
    }
}

bool CInviteSessionHandler::ResponsePlay(std::shared_ptr<CClientStream> stream, std::shared_ptr<PlayResponseParam> param)
{
    if (stream == nullptr) {
        ErrorL << "client play fail, requestId:" << param->strRequestId;
        return false;
    }

    if (!param->bOk) {
        WarnL << "start live failed";
        stream->GetInviteSessionHandle()->reject(500);
        RemoveClientStream(param->strRequestId);
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

// 接收上级的invite消息
void CInviteSessionHandler::onNewSession(resip::ServerInviteSessionHandle handle, resip::InviteSession::OfferAnswerType oat, const resip::SipMessage& msg)
{
    // 目前只允许一路
    {
        std::lock_guard<std::mutex> g(m_muxClientStream);
        if (m_mapClientStream.size() > 0) {
            handle->reject(400);
		    return;
        }
    }

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
        
    std::string strCallId;
	if (msg.exists(resip::h_CallId)) {
		strCallId = std::string(msg.header(resip::h_CallId).value().c_str());
	} else {
		handle->reject(400);
		return;
	}

    // 回复100trying
    handle->provisional(100);

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

    std::shared_ptr<CClientStream> client = std::make_shared<CClientStream>(this, param, m_poller);
    resip::SdpContents *pUasSdp = dynamic_cast<resip::SdpContents*>(sdp->clone());
    client->SetSdp(pUasSdp);
    client->SetServerInviteSessionHandle(handle);
    
    {
        InfoL << "new invite, streamId:" << param->strStreamId << ", callId:" << strCallId;

        std::lock_guard<std::mutex> g(m_muxClientStream);
        m_mapClientStream[param->strStreamId] = client;
    }

    client->StartPlay();
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

    return;
}

/// called when MESSAGE message is received 
void CInviteSessionHandler::onMessage(resip::InviteSessionHandle handle, const resip::SipMessage& msg)
{
    
}

}



