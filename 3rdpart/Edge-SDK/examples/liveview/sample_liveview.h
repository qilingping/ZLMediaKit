/**
 ********************************************************************
 *
 * @copyright (c) 2023 DJI. All rights reserved.
 *
 * All information contained herein is, and remains, the property of DJI.
 * The intellectual and technical concepts contained herein are proprietary
 * to DJI and may be covered by U.S. and foreign patents, patents in process,
 * and protected by trade secret or copyright law.  Dissemination of this
 * information, including but not limited to data and other proprietary
 * material(s) incorporated within the information, in any form, is strictly
 * prohibited without the express written consent of DJI.
 *
 * If you receive this source code without DJI’s authorization, you may not
 * further disseminate the information, and you must immediately remove the
 * source code and notify DJI of its removal. DJI reserves the right to pursue
 * legal actions against you for any loss(es) or damage(s) caused by your
 * failure to do so.
 *
 *********************************************************************
 */
#ifndef __SAMPLE_LIVEVIEW_H__
#define __SAMPLE_LIVEVIEW_H__

#include "error_code.h"
#include "image_processor.h"
#include "image_processor_thread.h"
#include "liveview.h"
#include "logger.h"
#include "stream_decoder.h"
#include "stream_processor_thread.h"

using ErrorCode = edge_sdk::ErrorCode;

namespace edge_app {

class LiveviewSample {
   public:
    static std::shared_ptr<LiveviewSample> CreateLiveview(
        const std::string& name, edge_sdk::Liveview::CameraType type,
        edge_sdk::Liveview::StreamQuality quality,
        std::shared_ptr<ImageProcessor> image_processor);

    LiveviewSample(const std::string& name);
    ~LiveviewSample() {}

    void SetProcessor(std::shared_ptr<StreamProcessorThread> processor);

    edge_sdk::ErrorCode Init(edge_sdk::Liveview::CameraType type,
                             edge_sdk::Liveview::StreamQuality quality);

    edge_sdk::ErrorCode Start();

    edge_sdk::ErrorCode SetCameraSource(edge_sdk::Liveview::CameraSource source);

   private:

    edge_sdk::ErrorCode StreamCallback(const uint8_t* data, size_t len);

    void LiveviewStatusCallback(
        const edge_sdk::Liveview::LiveviewStatus& status);

    enum {
        kLiveviewStatusTimeOutThreshold = 8,
    };

    bool try_restart_liveview_ = false;
    time_t received_liveview_status_time_ = 0;
    edge_sdk::Liveview::LiveviewStatus liveview_status_ = 0;
    std::shared_ptr<edge_sdk::Liveview> liveview_;
    std::shared_ptr<StreamProcessorThread> stream_processor_thread_;
};

}  // namespace edge_app

#endif
