#pragma once

extern "C" {
    #include <arv.h>
}

namespace cynlr {
namespace camera {

typedef struct FrameBuffer {
    void *parent_buffer = nullptr;
    void *data = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;
} FrameBuffer;

}  // namespace camera
}