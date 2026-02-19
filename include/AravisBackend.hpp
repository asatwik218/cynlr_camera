#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

extern "C" {
    #include <arv.h>
}

#include "CameraBackend.hpp"
#include "Frame.hpp"
#include "Stream.hpp"
#include "AravisStream.hpp"

namespace cynlr {
namespace camera {

using namespace std;

static const std::unordered_map<PixelFormat, ArvPixelFormat> pixel_format_map = {
    { PixelFormat::MONO8,          ARV_PIXEL_FORMAT_MONO_8 },
    { PixelFormat::MONO10,         ARV_PIXEL_FORMAT_MONO_10 },
    { PixelFormat::MONO12,         ARV_PIXEL_FORMAT_MONO_12 },
    { PixelFormat::MONO14,         ARV_PIXEL_FORMAT_MONO_14 },
    { PixelFormat::MONO16,         ARV_PIXEL_FORMAT_MONO_16 },
};

static const std::unordered_map<AcquisitionMode, ArvAcquisitionMode> acq_mode_map = {
    { AcquisitionMode::ACQUISITION_MODE_CONTINUOUS, ARV_ACQUISITION_MODE_CONTINUOUS },
    { AcquisitionMode::ACQUISITION_MODE_SINGLE_FRAME, ARV_ACQUISITION_MODE_SINGLE_FRAME },
    { AcquisitionMode::ACQUISITION_MODE_MULTI_FRAME, ARV_ACQUISITION_MODE_MULTI_FRAME }
};

class AravisBackend : public ICameraBackend {
public:
    ~AravisBackend();

    static unique_ptr<AravisBackend> create(
        const char* name,
        uint32_t stream_buffer_count = DEFAULT_NUM_BUFFERS);

    static std::vector<std::string> listCameras();

    optional<CamError> startAcquisition() override;
    optional<CamError> stopAcquisition() override;
    optional<CamError> setAcquisitionMode(AcquisitionMode mode) override;
    optional<CamError> setPixelFormat(PixelFormat format) override;
    optional<CamError> setBinning(int dx, int dy) override;
    optional<CamError> setGain(double gain) override;
    optional<CamError> setAutoExposure(bool setAuto) override;
    optional<CamError> setExposureTime(double exposure_time_us) override;
    optional<CamError> setFrameRate(double framerate) override;

    optional<CamError> enableLensPower(bool enable) override;
    optional<CamError> setupLensSerial(const char* baudRate) override;
    optional<CamError> setLensFocus(double voltage) override;

    shared_ptr<IStream> getStream() override;

private:
    AravisBackend(
        ArvCamera *camera,
        shared_ptr<AravisStream> stream,
        uint32_t stream_buffer_count = DEFAULT_NUM_BUFFERS) :
        camera(camera), stream(stream), stream_buffer_count(stream_buffer_count) {}

    optional<CamError> writeSerialFileAccess(const void* data, size_t length);

    ArvCamera *camera;
    shared_ptr<AravisStream> stream;
    uint32_t stream_buffer_count;
    GError *error = NULL;
    bool serial_port_open = false;
};

}  // namespace camera
}  // namespace cynlr