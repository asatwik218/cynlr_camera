#pragma once

#include <memory>
#include <optional>

#include "Constants.hpp"
#include "Frame.hpp"
#include "Error.hpp"
#include "CameraBackend.hpp"

namespace cynlr {
namespace camera {

using namespace std;
using namespace cynlr::camera;

inline void abortOnError(optional<CamError> error) {
    if (error.has_value()) {
        printf("Camera Error: %s\n", error->message);
        abort();
    }
}

inline void abortOnError(optional<StreamError> error) {
    if (error.has_value()) {
        printf("Stream Error: %s\n", error->message);
        abort();
    }
}

class Camera {
public:
    Camera(unique_ptr<ICameraBackend> backend) : m_backend(move(backend)) {}

    optional<CamError> startAcquisition();
    optional<CamError> stopAcquisition();
    optional<CamError> setAcquisitionMode(AcquisitionMode mode);
    optional<CamError> setPixelFormat(PixelFormat format);
    optional<CamError> setBinning(int dx, int dy);
    optional<CamError> setGain(double gain);
    optional<CamError> setAutoExposure(bool setAuto);
    optional<CamError> setExposureTime(double exposure_time_us);
    optional<CamError> setFrameRate(double framerate);
    optional<CamError> enableLensPower(bool enable);
    optional<CamError> setupLensSerial(
        const char* line, const char* source, const char* baudRate);
    optional<CamError> setLensFocus(double voltage);
    optional<StreamError> borrowOldestFrame(FrameBuffer &frame);
    optional<StreamError> borrowNewestFrame(FrameBuffer &frame);
    optional<StreamError> borrowNextNewFrame(FrameBuffer &frame);
    void releaseFrame(FrameBuffer &frame);

private:
    unique_ptr<ICameraBackend> m_backend;
};

}  // namespace camera
}  // namespace cynlr