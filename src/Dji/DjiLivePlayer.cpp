#include "Dji/DjiLivePlayer.h"
#include "Common/config.h"

using namespace toolkit;

namespace mediakit {

DjiLivePlayer::DjiLivePlayer(const toolkit::EventPoller::Ptr &poller)
    : _poller(poller)
    , _video_track(nullptr)
    , m_fp(nullptr)
    , m_frame_write(0)
{
    InfoL << "new.....";
}

DjiLivePlayer::~DjiLivePlayer()
{
    InfoL << "delete.....";
}


void DjiLivePlayer::play(const std::string &url)
{
    WarnL << url;

/*      // test code read from file
    m_poller = toolkit::EventPollerPool::Instance().getPoller();
    m_send_timer = std::make_shared<toolkit::Timer>((float)0.033, [this]() -> bool {
            testSendMedia();

            return true;
        }, m_poller);
    
    return ;
*/

    edge_sdk::ErrorCode errCode = liveInit();
    if (errCode != edge_sdk::kOk) {
        _poller->async_first([this, errCode](){
            toolkit::SockException err(Err_other); 
            WarnL << "live view init failed, err:" << (uint32_t)errCode;
            _on_play_result(err);
        });

        return;
    }

    errCode = liveStart();
    if (errCode != edge_sdk::kOk) {
        WarnL << "live view start failed, err:" << (uint32_t)errCode;
        teardown();

        _poller->async_first([this, errCode](){
            toolkit::SockException err(Err_other); 
            _on_play_result(err);
        });

        return;
    }
}

void DjiLivePlayer::pause(bool pause)
{
    WarnL << "dji player not support pause";
}

void DjiLivePlayer::speed(float speed) 
{
    WarnL << "dji player not support speed";
}

void DjiLivePlayer::teardown()
{
    WarnL << "dji player terdown";


    if (_media_src) {
        InfoL << "media source unregist";
        _media_src->unregist(); 
    }

    liveStop();

    liveUninit();
}


float DjiLivePlayer::getPacketLossRate(TrackType type) const
{
    return (float)0.0;
}


std::vector<Track::Ptr> DjiLivePlayer::getTracks(bool ready /*= true*/) const 
{
    std::vector<Track::Ptr> tracks;
    tracks.push_back(_video_track);

    return std::move(tracks);
}

void DjiLivePlayer::setMediaSource(const MediaSource::Ptr &src)
{ 
    _media_src = src; 

    InfoL << "DjiLivePlayer::setMediaSource, enter";

    if (_media_src) {
        InfoL << "than media regist";
        _media_src->regist(); 
    }
}


edge_sdk::ErrorCode DjiLivePlayer::streamCallback(const uint8_t* data, size_t len) 
{
    // 创建h.264 track
    if (!_video_track) {
        _video_track = std::make_shared<H264Track>();

        // 仅仅通知一次
        _poller->async_first([this](){
            InfoL << "live view start success, call play result";
            toolkit::SockException err; 
            _on_play_result(err);
        });
    }

/*      // debug info
    char header[128] = {0};
    snprintf(header, 127, "header 0x%x 0x%x 0x%x 0x%x 0x%x, frame_len=%d\n", data[0], data[1], data[2], data[3], data[4], len);
    InfoL << header;
*/

    // 0x00 0x00 0x00 0x01 0x67 ...
    H264Frame::Ptr frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    frame->_buffer.assign((const char*)data, len);

    _poller->async([this, frame](){
        _video_track->inputFrame(frame);
    });

    return edge_sdk::kOk;
}

edge_sdk::ErrorCode DjiLivePlayer::liveInit() {
    // 获取摄像头参数
    GET_CONFIG(uint32_t, cameraType, Dji::kCameraType);
    GET_CONFIG(uint32_t, streamQuality, Dji::kStreamQuality);
    GET_CONFIG(uint32_t, cameraSourceWide, Dji::kCameraSourceWide);

    InfoL << "cameraType:" << cameraType << ", streamQuality:" << streamQuality << ", cameraSourceWide:" << cameraSourceWide;
    
    auto callback_main =
        std::bind(&DjiLivePlayer::streamCallback, this, std::placeholders::_1,
                  std::placeholders::_2);
    edge_sdk::Liveview::Options option = {(edge_sdk::Liveview::CameraType)cameraType, 
                                            (edge_sdk::Liveview::StreamQuality)streamQuality, 
                                            callback_main};

    liveview_ = edge_sdk::CreateLiveview();

    auto rc = liveview_->Init(option);

    liveview_->SubscribeLiveviewStatus(std::bind(
        &DjiLivePlayer::liveviewStatusCallback, this, std::placeholders::_1));

    liveview_->SetCameraSource((edge_sdk::Liveview::CameraSource)cameraSourceWide);

    return rc;
}

void DjiLivePlayer::liveUninit()
{
    InfoL << "uninit";

    if (liveview_) {
        liveview_->DeInit();
    }
}

edge_sdk::ErrorCode DjiLivePlayer::liveStart() {

    auto rc = liveview_->StartH264Stream();
    if (rc == edge_sdk::kOk) {
        play_success_ = true;
    }
    return rc;
}

void DjiLivePlayer::liveStop()
{
    InfoL << "liveStop";

    if (liveview_ &&play_success_) {
        liveview_->StopH264Stream();
    }
}

void DjiLivePlayer::liveviewStatusCallback(const edge_sdk::Liveview::LiveviewStatus& status) 
{
    auto now = time(NULL);
    if (now >
        kLiveviewStatusTimeOutThreshold + received_liveview_status_time_) {
        try_restart_liveview_ = true;
    }

    if (status != liveview_status_) {
        if (liveview_status_ == 0) {
            try_restart_liveview_ = true;
        }
        liveview_status_ = status;
    }

    if (status != 0 && try_restart_liveview_) {
        InfoL << "Restart h264 stream...";
        auto rc = liveview_->StartH264Stream();
        if (rc == edge_sdk::kOk) {
            try_restart_liveview_ = false;
        } else {
            ErrorL << "restart failed: " << rc;
            try_restart_liveview_ = true;
        }
    }
    received_liveview_status_time_ = now;
}

void DjiLivePlayer::testSendMedia()
{
    if (m_fp == nullptr) {
        m_fp = fopen("/home/dji/ZLMediaKit/test_media/test_media_2.raw", "rb");

        m_frame = new uint8_t[100*1024*1024];       // new 1M
        int32_t ret = fread(m_frame, 23*1024*1024, 1, m_fp);
    }

    uint8_t prefixI[5] = {0x00, 0x00, 0x00, 0x01, 0x65};
    uint8_t prefixP[5] = {0x00, 0x00, 0x00, 0x01, 0x61};

    int32_t i = m_frame_write + 5;

    for (i; i < 23*1024*1024; i++) {
        if ((memcmp(&m_frame[i], prefixI, 5) == 0) || (memcmp(&m_frame[i], prefixP, 5) == 0)) {
            break;
        }
    }

    // 当前 i的位置从0x00 0x00开始  m_frame_write~i之间为一帧的大小
    uint32_t frame_len = i - m_frame_write;

//    WarnL << "pos i = " << i << ", m_frame_write=" << m_frame_write << ", frame_len=" << frame_len;

    streamCallback((const uint8_t*)(m_frame + m_frame_write), frame_len);

    m_frame_write = i;
}

}/* namespace mediakit */