#include "Dji/DjiLivePlayer.h"

using namespace toolkit;

namespace mediakit {

DjiLivePlayer::DjiLivePlayer(const toolkit::EventPoller::Ptr &poller)
    : _poller(poller)
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

    setCameraSource(edge_sdk::Liveview::kCameraSourceWide);

    edge_sdk::ErrorCode errCode = liveInit(edge_sdk::Liveview::kCameraTypePayload, edge_sdk::Liveview::kStreamQuality720p);
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
        _poller->async_first([this, errCode](){
            toolkit::SockException err(Err_other); 
            WarnL << "live view start failed, err:" << (uint32_t)errCode;
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

    if (liveview_) {
        if (play_success_) {
            liveview_->StopH264Stream();
        }

        liveview_->DeInit();
    }
}

edge_sdk::ErrorCode DjiLivePlayer::setCameraSource(edge_sdk::Liveview::CameraSource source)
{
    
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
}


edge_sdk::ErrorCode DjiLivePlayer::streamCallback(const uint8_t* data, size_t len) 
{
    // 创建h.264 track
    if (!_video_track) {
        _video_track = std::make_shared<H264Track>();
    }

    play_success_ = true;

    // play success
    _poller->async([this](){
        WarnL << "live view start success, call play result";
        toolkit::SockException err; 
        _on_play_result(err);
    });

    // 0x00 0x00 0x00 0x01 0x67 ...
    H264Frame::Ptr frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    frame->_buffer.assign((const char*)data, len);

    _poller->async([this, frame](){
        _video_track->inputFrame(frame);
    });

    return edge_sdk::kOk;
}

edge_sdk::ErrorCode DjiLivePlayer::liveInit(edge_sdk::Liveview::CameraType type,
                               edge_sdk::Liveview::StreamQuality quality) {
    auto callback_main =
        std::bind(&DjiLivePlayer::streamCallback, this, std::placeholders::_1,
                  std::placeholders::_2);
    edge_sdk::Liveview::Options option = {type, quality, callback_main};

    liveview_ = edge_sdk::CreateLiveview();

    auto rc = liveview_->Init(option);

    liveview_->SubscribeLiveviewStatus(std::bind(
        &DjiLivePlayer::liveviewStatusCallback, this, std::placeholders::_1));

    return rc;
}

edge_sdk::ErrorCode DjiLivePlayer::liveStart() {

    auto rc = liveview_->StartH264Stream();
    return rc;
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

}/* namespace mediakit */