#include "Dji/DjiLivePlayer.h"
#include "Common/config.h"

using namespace toolkit;

namespace mediakit {

DjiLivePlayer::DjiLivePlayer(const MediaTuple& tuple, const ProtocolOption& option)
    : m_fp(nullptr)
    , m_frame_write(0)
{
    InfoL << "new.....";

    _poller = _poller == nullptr ? toolkit::EventPollerPool::Instance().getPoller() : _poller;

    _tuple = tuple;
    _option = option;

    _media_src = std::make_shared<DjiMediaSource>(tuple, _poller);
    _video_track = std::make_shared<H264Track>();
    _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, 0.0, _option);

    _media_src->setListener(_muxer);
    _media_src->regist();

    _muxer->addTrack(_video_track);
    // 视频数据写入_mediaMuxer
    _video_track->addDelegate(_muxer);
    // 添加完毕所有track，防止单track情况下最大等待3秒
    _muxer->addTrackCompleted();
    
}

DjiLivePlayer::~DjiLivePlayer()
{
    InfoL << "delete.....";

    teardown();
}


int32_t DjiLivePlayer::play()
{   
    _poller->async([this]() {
        
        pthread_t threadId = pthread_self();
        InfoL << threadId << " | dji play";

        if (play_success_) {
            // 直接return
            InfoL << threadId << " | dji playing";
            return 0;
        }

#if 0       // test code read from file
        m_send_timer = std::make_shared<toolkit::Timer>((float)0.33, [this]() -> bool {
                testSendMedia();

                return true;
            }, nullptr);
        
        return 0;
#endif

        if (!init_) {
            edge_sdk::ErrorCode errCode = liveInit();
            if (errCode != edge_sdk::kOk) {
                ErrorL << "live init failed, errCode:" << errCode;
                return -1;
            }

            init_ = true;
        }
        

        edge_sdk::ErrorCode errCode = liveStart();
        if (errCode != edge_sdk::kOk) {
            ErrorL << "live start failed, errCode:" << errCode;
            return -1;
        }

    });

    return 0;
}

void DjiLivePlayer::teardown()
{
    _poller->async([this](){
        pthread_t threadId = pthread_self();
        InfoL << threadId << " | dji player terdown";

        liveStop();
        liveUninit();

        if (_media_src) {
            InfoL << "media source unregist";
            _media_src->unregist(); 
        }
        _media_src.reset();
        _muxer.reset();
        _video_track.reset();
    });
    
}


edge_sdk::ErrorCode DjiLivePlayer::streamCallback(const uint8_t* data, size_t len) 
{
    pthread_t threadId = pthread_self();
//    InfoL <<  threadId << " | stream callback..";
/*      // debug info
    char header[128] = {0};
    snprintf(header, 127, "header 0x%x 0x%x 0x%x 0x%x 0x%x, frame_len=%d\n", data[0], data[1], data[2], data[3], data[4], len);
    InfoL << header;
*/

    // 0x00 0x00 0x00 0x01 0x67 ...
    H264Frame::Ptr frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    frame->_buffer.assign((const char*)data, len);

    std::weak_ptr<DjiLivePlayer> weakSelf = shared_from_this();

    _poller->async([weakSelf, frame](){
        DjiLivePlayer::Ptr strongSelf = weakSelf.lock();
        if (strongSelf != nullptr) {
            Track::Ptr video_track = strongSelf->getVideoTrack();
            if (video_track) {
                pthread_t threadId = pthread_self();
            //    InfoL << threadId << " | input frame..";
                video_track->inputFrame(frame);
            }
        }
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
    pthread_t threadId = pthread_self();
    InfoL << threadId << " | uninit, enter...";

    if (liveview_ && init_) {
        liveview_->DeInit();
    }

    InfoL << threadId << " | uninit, liveview reset doing...";
    liveview_.reset();
    InfoL << threadId << " | uninit, liveview reset leave...";
}

edge_sdk::ErrorCode DjiLivePlayer::liveStart() 
{
    InfoL << "liveStart, stream:" << _tuple.stream;

    play_success_ = false;
    
    auto rc = liveview_->StartH264Stream();
    if (rc == edge_sdk::kOk) {
        play_success_ = true;
    }
    return rc;
}

void DjiLivePlayer::liveStop()
{
    InfoL << "liveStop, stream:" << _tuple.stream;

    if (liveview_) {
        if (play_success_) {
            liveview_->StopH264Stream();
            play_success_ = false;
        } else {
            InfoL << "play failed, do nothing, stream:" << _tuple.stream;
        }
    }
}

void DjiLivePlayer::liveviewStatusCallback(const edge_sdk::Liveview::LiveviewStatus& st) 
{
    edge_sdk::Liveview::LiveviewStatus status = st;
    _poller->async([this, status]() {
        
        pthread_t threadId = pthread_self();
        InfoL << threadId << "status = " << (int32_t)status << ", liveview_status_ = " << (int32_t)liveview_status_;

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
            InfoL << threadId << "Restart h264 stream..., status = " << (int32_t)status;
            auto rc = liveStart();
            if (rc == edge_sdk::kOk) {
                try_restart_liveview_ = false;
            } else {
                ErrorL << "restart failed: " << rc;
                try_restart_liveview_ = true;
            }
        }
        received_liveview_status_time_ = now;
    });

}

void DjiLivePlayer::testSendMedia()
{
    if (m_fp == nullptr) {
        m_fp = fopen("/home/dji/ZLMediaKit/test_media/test_media_2.raw", "rb");
        if (!m_fp) {
            WarnL << "test media open failed";
            return;
        }
        InfoL << "test media";
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