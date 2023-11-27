#include "Dji/DjiLivePlayer.h"

using namespace toolkit;

namespace mediakit {

DjiLivePlayer::DjiLivePlayer(const toolkit::EventPoller::Ptr &poller)
    : _poller(poller)
{
    WarnL << "new.....";
}

DjiLivePlayer::~DjiLivePlayer()
{

}


void DjiLivePlayer::play(const std::string &url)
{
    WarnL << url;
    if (!_video_track) {
        _video_track = std::make_shared<H264Track>();
    }

    _poller->async([this](){
        toolkit::SockException err; 
        WarnL << "exec result";
        _on_play_result(err);
    });
}

void DjiLivePlayer::pause(bool pause)
{

}

void DjiLivePlayer::speed(float speed) 
{

}

void DjiLivePlayer::teardown()
{
    
}

float DjiLivePlayer::getPacketLossRate(TrackType type) const
{

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


edge_sdk::ErrorCode DjiLivePlayer::streamCallback(const uint8_t* data, size_t len) {

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