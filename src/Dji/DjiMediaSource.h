#ifndef SRC_DJI_DJIMEDIASOURCE_H_
#define SRC_DJI_DJIMEDIASOURCE_H_

#include <string>
#include <memory>
#include "Common/MediaSource.h"

namespace mediakit {

class DjiMediaSource : public MediaSource {
public:
    DjiMediaSource(const MediaTuple& tuple) :  MediaSource("dji", tuple) {}
    virtual ~DjiMediaSource() {}

public:
    int readerCount() { return 0; }
};

}



#endif // SRC_DJI_DJIMEDIASOURCE_H_