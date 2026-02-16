#include "AravisStream.hpp"

using namespace std;
using namespace cynlr::camera;

optional<StreamError> AravisStream::borrowOldestFrame(FrameBuffer &frame) {
    int output_buffer_count;
    
    arv_stream_get_n_buffers(m_stream, NULL, &output_buffer_count);

    /* Discard all but the last buffer */
    for (int i = 0; i < output_buffer_count-1; i++) {
        ArvBuffer *buffer = arv_stream_pop_buffer(m_stream);
        arv_stream_push_buffer(m_stream, buffer);
    }

    return populateFrameBuffer(frame);
}

optional<StreamError> AravisStream::borrowNewestFrame(FrameBuffer &frame) {
    return populateFrameBuffer(frame);
}

optional<StreamError> AravisStream::borrowNextNewFrame(FrameBuffer &frame) {
    int output_buffer_count;
    
    arv_stream_get_n_buffers(m_stream, NULL, &output_buffer_count);

    /* Discard ALL buffers */
    for (int i = 0; i < output_buffer_count; i++) {
        ArvBuffer *buffer = arv_stream_pop_buffer(m_stream);
        arv_stream_push_buffer(m_stream, buffer);
    }

    return populateFrameBuffer(frame);
}

void AravisStream::releaseFrame(FrameBuffer &frame) {
    arv_stream_push_buffer(m_stream, static_cast<ArvBuffer*>(frame.parent_buffer));
}

optional<StreamError> AravisStream::populateFrameBuffer(FrameBuffer &frame) {
    ArvBuffer *buffer = arv_stream_pop_buffer(m_stream);
    if (ARV_IS_BUFFER(buffer)) {
        if (arv_buffer_get_status(buffer) != ARV_BUFFER_STATUS_SUCCESS) {
            printf("Warning: Borrowed frame has status %d\n", arv_buffer_get_status(buffer));
            return StreamError { .message = "Buffer population failed" };
        }
        size_t size;
        frame.parent_buffer = static_cast<void*>(buffer);
        frame.data = const_cast<void*>(arv_buffer_get_data(buffer, &size));
        frame.width = arv_buffer_get_image_width(buffer);
        frame.height = arv_buffer_get_image_height(buffer);

        return nullopt;
    }

    return StreamError { .message = "Failed to populate frame buffer" };
}