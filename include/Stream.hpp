#pragma once

#include <optional>
#include "Error.hpp"
#include "Frame.hpp"

namespace cynlr {
namespace camera {

using namespace std;
 
class IStream {
public:
    /* Borrow the oldest frame from the stream. It will discard all buffers 
     * except the oldest one.
     *
     * @param frame The frame buffer to fill.
     * @return An error if the operation failed, or nullopt if successful. */
    virtual optional<StreamError> borrowOldestFrame(FrameBuffer &frame) = 0;

    /* Borrow the newest frame from the stream.
     *
     * @param frame The frame buffer to fill.
     * @return An error if the operation failed, or nullopt if successful. */
    virtual optional<StreamError> borrowNewestFrame(FrameBuffer &frame) = 0;

    /* Borrow the next new frame from the stream. 
     * It will discard all buffers and will block until a new frame is available.
     *
     * @param frame The frame buffer to fill.
     * @return An error if the operation failed, or nullopt if successful. */
    virtual optional<StreamError> borrowNextNewFrame(FrameBuffer &frame) = 0;

    /* Release a previously borrowed frame back to the stream.
     * This MUST be called for each frame borrowed.
     *
     * @param frame The frame buffer to release. */
    virtual void releaseFrame(FrameBuffer &frame) = 0;
};

}
}