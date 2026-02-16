#pragma once

namespace cynlr {
namespace camera {

enum class AcquisitionMode {
    ACQUISITION_MODE_CONTINUOUS = 0,
    ACQUISITION_MODE_SINGLE_FRAME = 1,
    ACQUISITION_MODE_MULTI_FRAME = 2,
};

enum class PixelFormat {
    MONO8 = 0,
    MONO10 = 1,
    MONO12 = 2,
    MONO14 = 3,
    MONO16 = 4,
    // TODO : many more to add
};

}
}