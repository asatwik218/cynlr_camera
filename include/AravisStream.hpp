#include <memory>
#include <optional>
#include "Stream.hpp"

extern "C" {
    #include <arv.h>
}

#define DEFAULT_NUM_BUFFERS 10

namespace cynlr {
namespace camera {

using namespace std;

class AravisStream : public IStream {
public:

    /* Constructor for AravisStream.
     *
     * @param stream The ArvStream to wrap. */
    AravisStream(ArvStream *stream) : m_stream(stream) {}
    ~AravisStream() {
        g_clear_object(&m_stream);
    };

    optional<StreamError> borrowOldestFrame(FrameBuffer &frame) override;
    optional<StreamError> borrowNewestFrame(FrameBuffer &frame) override;
    optional<StreamError> borrowNextNewFrame(FrameBuffer &frame) override;
    void releaseFrame(FrameBuffer &frame) override;

    ArvStream *m_stream;

private:
    optional<StreamError> populateFrameBuffer(FrameBuffer &frame);
};
 
}  // namespace camera 
}  // namespace cynlr