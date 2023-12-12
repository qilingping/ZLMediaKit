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
#include "Common/MultiMediaSourceMuxer.h"
#include "Player/MediaPlayer.h"
#include "DjiMediaSource.h"

#include "Edge-SDK/include/init.h"
#include "Edge-SDK/include/error_code.h"
#include "Edge-SDK/include/logger.h"
#include "Edge-SDK/include/liveview.h"

namespace mediakit {

//实现了dji esdk部分的功能，及数据接收功能
class DjiLivePlayer : public std::enable_shared_from_this<DjiLivePlayer> {
public:
    using Ptr = std::shared_ptr<DjiLivePlayer>;

    DjiLivePlayer(const MediaTuple& tuple, const ProtocolOption& option);
    ~DjiLivePlayer();

    int32_t play();
    void teardown();

    Track::Ptr getVideoTrack() { return _video_track; }

private:
    edge_sdk::ErrorCode liveInit();
    void liveUninit();

    edge_sdk::ErrorCode liveStart();
    void liveStop();

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
    bool init_ = false;

private:
    toolkit::EventPoller::Ptr _poller;
    MediaSource::Ptr _media_src;
    MultiMediaSourceMuxer::Ptr _muxer;
    ProtocolOption _option;
    MediaTuple _tuple;

    Track::Ptr _video_track;

    // test
    toolkit::Timer::Ptr m_send_timer;
    FILE* m_fp;
    uint8_t* m_frame;
    uint32_t m_frame_write;
};

} /* namespace mediakit */


#endif // SRC_DJI_DJILIVEPLAYER_H_