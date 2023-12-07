#ifndef SRC_DJI_DJIMEDIASOURCE_H_
#define SRC_DJI_DJIMEDIASOURCE_H_

#include <string>
#include <memory>
#include "Common/MediaSource.h"
#include "Common/macros.h"

#include "Util/logger.h"

namespace mediakit {

class DjiMediaSource : public MediaSource {
public:
    DjiMediaSource(const MediaTuple& tuple, toolkit::EventPoller::Ptr poller) 
        :  MediaSource(DJI_SCHEMA, tuple) 
        , m_poller(poller)
    {
        InfoL << "";
    }
    virtual ~DjiMediaSource() {}

public:
    int readerCount() { return 0; }

    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) {
        return m_poller;
    }

private:
    toolkit::EventPoller::Ptr m_poller;
};

}



#endif // SRC_DJI_DJIMEDIASOURCE_H_