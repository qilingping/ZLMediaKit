#ifndef SRC_DJI_DJILIVEPLAYER_H_
#define SRC_DJI_DJILIVEPLAYER_H_

#include <string>
#include <memory>
#include <stdio.h>
#include <string.h>

#include "Util/TimeTicker.h"
#include "Poller/Timer.h"
#include "Poller/EventPoller.h"
#include "Player/PlayerBase.h"
#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "Extension/H264.h"

#include "Edge-SDK/include/init.h"
#include "Edge-SDK/include/error_code.h"
#include "Edge-SDK/include/logger.h"
#include "Edge-SDK/include/liveview.h"

namespace mediakit {

//实现了rtsp播放器协议部分的功能，及数据接收功能
class DjiLivePlayer : public PlayerBase {
public:
    using Ptr = std::shared_ptr<DjiLivePlayer>;

    DjiLivePlayer(const toolkit::EventPoller::Ptr &poller);
    ~DjiLivePlayer() override;

    void play(const std::string &strUrl) override;
    void pause(bool pause) override;
    void speed(float speed) override;
    void teardown() override;
    float getPacketLossRate(TrackType type) const override;

    float getProgress() const override { return 0.0;}

    uint32_t getProgressPos() const override { return 0;}

    void seekTo(float fProgress) override {return;}

    void seekTo(uint32_t seekPos) override { return ;}

    float getDuration() const override { return 0.0;};

    std::vector<Track::Ptr> getTracks(bool ready = true) const override;

    void setMediaSource(const MediaSource::Ptr &src) override;

    void setOnShutdown(const std::function<void(const toolkit::SockException &)> &cb) override {
        _on_shutdown = cb;
    }

    void setOnPlayResult(const std::function<void(const toolkit::SockException &ex)> &cb) override {
        _on_play_result = cb;
    }

    void setOnResume(const std::function<void()> &cb) override {
        _on_resume = cb;
    }

protected:
    void onShutdown(const toolkit::SockException &ex) override {
        if (_on_shutdown) {
            _on_shutdown(ex);
            _on_shutdown = nullptr;
        }
    }

    void onPlayResult(const toolkit::SockException &ex) override {
        if (_on_play_result) {
            _on_play_result(ex);
            _on_play_result = nullptr;
        }
    }

    void onResume() override {
        if (_on_resume) {
            _on_resume();
        }
    }


private:
    bool addTrack(const Track::Ptr &track) { return true; }

    void addTrackCompleted() { return; };

private:
    edge_sdk::ErrorCode liveInit(edge_sdk::Liveview::CameraType type,
                             edge_sdk::Liveview::StreamQuality quality);

    edge_sdk::ErrorCode liveStart();

    edge_sdk::ErrorCode setCameraSource(edge_sdk::Liveview::CameraSource source);

    edge_sdk::ErrorCode streamCallback(const uint8_t* data, size_t len);

    void liveviewStatusCallback(const edge_sdk::Liveview::LiveviewStatus& status);

    enum {
        kLiveviewStatusTimeOutThreshold = 8,
    };

private:
    void testSendMedia();
    
private:

    bool try_restart_liveview_ = false;
    time_t received_liveview_status_time_ = 0;
    edge_sdk::Liveview::LiveviewStatus liveview_status_ = 0;
    std::shared_ptr<edge_sdk::Liveview> liveview_ = nullptr;

    bool play_success_ = false;

private:
    toolkit::EventPoller::Ptr _poller;
    MediaSource::Ptr _media_src;
    std::function<void()> _on_resume;
    PlayerBase::Event _on_shutdown;
    PlayerBase::Event _on_play_result;

    Track::Ptr _video_track;

    // test
    toolkit::EventPoller::Ptr m_poller;
    toolkit::Timer::Ptr m_send_timer;
    FILE* m_fp;
    uint8_t* m_frame;
    uint32_t m_frame_write;
};

} /* namespace mediakit */


#endif // SRC_DJI_DJILIVEPLAYER_H_