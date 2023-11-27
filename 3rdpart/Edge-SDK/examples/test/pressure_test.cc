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
#include <unistd.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <thread>

#include "../liveview/image_processor_thread.h"
#include "../liveview/sample_liveview.h"
#include "../liveview/stream_decoder.h"
#include "../liveview/stream_processor_thread.h"
#include "image_processor.h"
#include "liveview.h"
#include "logger.h"
#include "media_manager.h"
#include "opencv2/dnn.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/opencv.hpp"

using namespace cv;
using namespace edge_sdk;
using namespace edge_app;

namespace {

std::queue<MediaFile> media_file_queue;
std::mutex media_file_queue_mutex;
std::condition_variable media_file_queue_cv;

std::shared_ptr<MediaFilesReader> media_files_reader;

char current_path_[128];
char media_file_save_root_dir[256];

}  // namespace

ErrorCode ESDKInit();

static ErrorCode ReadMediaFile(const MediaFile& file,
                               std::vector<uint8_t>& image) {
    char buf[1024 * 1024];

    auto fd = media_files_reader->Open(file.file_path);
    if (fd < 0) {
        return kErrorSystemError;
    }
    do {
        auto nread = media_files_reader->Read(fd, buf, sizeof(buf));
        if (nread > 0) {
            image.insert(image.end(), (uint8_t*)buf, (uint8_t*)(buf + nread));
        } else {
            media_files_reader->Close(fd);
            break;
        }
    } while (1);

    INFO("filesize: %d, image_vec: %d", file.file_size, image.size());

    return kOk;
}

static ErrorCode ReadMediaFile(const char* file_path) {
    char buf[1024 * 1024];
    std::vector<uint8_t> image;

    auto fd = media_files_reader->Open(file_path);
    if (fd < 0) {
        return kErrorSystemError;
    }
    do {
        auto nread = media_files_reader->Read(fd, buf, sizeof(buf));
        if (nread > 0) {
            image.insert(image.end(), (uint8_t*)buf, (uint8_t*)(buf + nread));
        } else {
            media_files_reader->Close(fd);
            break;
        }
    } while (1);

    INFO("image_vec: %d", image.size());

    return kOk;
}

static ErrorCode GetCurrentFileDirPath(const char* filePath,
                                       uint32_t pathBufferSize, char* dirPath) {
    uint32_t i = strlen(filePath) - 1;
    uint32_t dirPathLen;

    while (filePath[i] != '/') {
        i--;
    }

    dirPathLen = i + 1;

    if (dirPathLen + 1 > pathBufferSize) {
        return kErrorParamOutOfRange;
    }

    memcpy(dirPath, filePath, dirPathLen);
    dirPath[dirPathLen] = 0;

    return kOk;
}

static void DumpFiles(const std::string& filename,
                      const std::vector<uint8_t>& data) {
    std::string write_file =
        std::string(media_file_save_root_dir) + std::string("/") + filename;
    FILE* f = fopen(write_file.c_str(), "wb");
    if (f) {
        fwrite(data.data(), data.size(), 1, f);
        fclose(f);
    }
}

static void ProcessMediaFile() {
    std::vector<uint8_t> image;
    while (1) {
        std::unique_lock<std::mutex> l(media_file_queue_mutex);

        media_file_queue_cv.wait(l,
                                 [] { return media_file_queue.size() != 0; });

        MediaFile file = media_file_queue.front();
        media_file_queue.pop();
        INFO("process media file:%s", file.file_name.c_str());
        l.unlock();
        auto rc = ReadMediaFile(file, image);
        if (rc == kOk) {
            DumpFiles(file.file_name, image);
        }
        image.clear();
    }
}

static ErrorCode MediaFilesUpdateCallback(const MediaFile& file) {
    INFO("File update: %s", file.file_name.c_str());
    std::lock_guard<std::mutex> l(media_file_queue_mutex);
    media_file_queue.push(file);
    media_file_queue_cv.notify_one();
    return kOk;
}

static void StartDumpMediaFiles() {
    snprintf(media_file_save_root_dir, sizeof(media_file_save_root_dir),
             "%s../../build/%s", current_path_, "media_files");
    char cmd[532];
    snprintf(cmd, sizeof(cmd), "[ -d %s] || mkdir %s -p",
             media_file_save_root_dir, media_file_save_root_dir);
    auto ret = system(cmd);
    if (ret != 0) {
        WARN("mkdir %s failed", media_file_save_root_dir);
        snprintf(media_file_save_root_dir, sizeof(media_file_save_root_dir),
                 "%s", current_path_);
    }
    INFO("media file saved directory: %s", media_file_save_root_dir);

    media_files_reader = MediaManager::Instance()->CreateMediaFilesReader();
    auto rc = media_files_reader->Init();
    if (rc != kOk) {
        ERROR("init media files reader error");
        return;
    }

    auto instance = MediaManager::Instance();
    instance->RegisterMediaFilesObserver(
        std::bind(MediaFilesUpdateCallback, std::placeholders::_1));

    auto t = std::thread(&ProcessMediaFile);
    t.detach();

    std::thread([] {
        while (1) {
            MediaManager::Instance()->SetDroneNestAutoDelete(false);
            sleep(5);
        }
    }).detach();
}

class JpegRecordProcessor : public ImageProcessor {
   public:
    JpegRecordProcessor(const std::string& name) : name_(name) {
        snprintf(file_path_, sizeof(file_path_), "%s../../build/%s",
                 current_path_, "video2jpeg");
        char cmd[532];
        snprintf(cmd, sizeof(cmd), "[ -d %s] || mkdir %s -p", file_path_,
                 file_path_);
        auto ret = system(cmd);
        if (ret != 0) {
            WARN("mkdir %s failed", file_path_);
        }
        INFO("jpeg recorder init successfully");
    }

    ~JpegRecordProcessor() override {}

    void Process(const std::shared_ptr<Image> image) override {
        frame_counter_++;
        if (frame_counter_ > 150) {
            frame_counter_ = 0;
            char buf[32];
            auto now = time(NULL);
            strftime(buf, sizeof(buf), "_%Y-%m-%d-%H-%M-%S", localtime(&now));
            std::string file = file_path_ + std::string("/") + name_ +
                               std::string(buf) + ".jpg";
            cv::imwrite(file.c_str(), *image);
            INFO("write image: %s", file.c_str());
        }
    }

   protected:
   private:
    uint32_t frame_counter_ = 0;
    std::string name_;
    char file_path_[256];
};

int main(int argc, char** argv) {
    auto rc = ESDKInit();
    if (rc != kOk) {
        ERROR("pre init failed");
        return -1;
    }

    if (GetCurrentFileDirPath(__FILE__, sizeof(current_path_), current_path_) !=
        kOk) {
        WARN("get path failed");
        snprintf(current_path_, sizeof(current_path_), "/tmp/");
    }

    StartDumpMediaFiles();

    auto image_processor = std::make_shared<JpegRecordProcessor>("Payload");
    auto liveview = edge_app::LiveviewSample::CreateLiveview(
        "Payload", Liveview::kCameraTypePayload, Liveview::kStreamQuality720p,
        image_processor);

    rc = liveview->Start();
    if (rc != kOk) {
        ERROR("liveview start: %d", rc);
    }

    INFO("start FPV cammera");
    auto fpv_image_processor = std::make_shared<JpegRecordProcessor>("FPV");
    auto fpv_liveview = edge_app::LiveviewSample::CreateLiveview(
        "FPV", Liveview::kCameraTypeFpv, Liveview::kStreamQuality720p,
        fpv_image_processor);

    rc = fpv_liveview->Start();
    if (rc != kOk) {
        ERROR("%d", rc);
    }

    if (argc == 3) {
        uint32_t times = atoi(argv[2]);
        for (int i = 0; i < times; i++) {
            rc = ReadMediaFile(argv[1]);
            INFO("Read Media File Test result: %d, times: %d", rc, i);
        }
    }

    while (1) sleep(3);

    return 0;
}
