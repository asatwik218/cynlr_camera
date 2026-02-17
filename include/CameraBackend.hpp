#pragma once

#include <optional>

#include "Error.hpp"
#include "Constants.hpp"
#include "Frame.hpp"
#include "Stream.hpp"

namespace cynlr {
namespace camera {

using namespace std;

class ICameraBackend {
public:
    virtual optional<CamError> startAcquisition() = 0;
    virtual optional<CamError> stopAcquisition() = 0;
    virtual optional<CamError> setAcquisitionMode(AcquisitionMode mode) = 0;
    virtual optional<CamError> setPixelFormat(PixelFormat format) = 0;
    virtual optional<CamError> setBinning(int dx, int dy) = 0;
    virtual optional<CamError> setGain(double gain) = 0;
    virtual optional<CamError> setAutoExposure(bool setAuto) = 0;
    virtual optional<CamError> setExposureTime(double exposure_time_us) = 0;
    virtual optional<CamError> setFrameRate(double framerate) = 0;

    virtual optional<CamError> enableLensPower(bool enable) = 0;
    virtual optional<CamError> setupLensSerial(
        const char* line, const char* source, const char* baudRate) = 0;
    virtual optional<CamError> setLensFocus(double voltage) = 0;

    virtual shared_ptr<IStream> getStream() = 0;
};

}  // namespace camera 
}  // namespace cynlr