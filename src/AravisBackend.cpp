#include <memory>
#include <algorithm>
#include <cstdint>

#include "AravisBackend.hpp"

#define ARV_RET_OPT(expr, error)                          \
    do {                                                  \
        (expr);                                           \
        if (error != NULL) {                              \
            return CamError { .message = error->message };\
        } else {                                          \
            return nullopt;                               \
        }                                                 \
    } while (0)

#define ARV_CHECK_ERROR(err)                              \
    do {                                                  \
        if (err != NULL) {                                \
            CamError e { .message = err->message };       \
            g_clear_error(&err);                          \
            return e;                                     \
        }                                                 \
    } while (0)

namespace cynlr {
namespace camera {

using namespace std;

unique_ptr<AravisBackend> AravisBackend::create(
    const char* name,
    uint32_t stream_buffer_count)
{
    GError *error;

    ArvCamera *new_camera = arv_camera_new(name, &error);
    if (!ARV_IS_CAMERA(new_camera)) {
        return nullptr;
    }

    ArvStream *new_stream = arv_camera_create_stream(new_camera, NULL, NULL, &error);
    if (!ARV_IS_STREAM(new_stream)) {
        printf("Error : Stream not created at %s:%d\n", __FILE__, __LINE__);
        return nullptr;
    }

    AravisBackend *backend = new AravisBackend(
        new_camera,
        make_shared<AravisStream>(new_stream),
        stream_buffer_count);

    return unique_ptr<AravisBackend>(backend);
}

AravisBackend::~AravisBackend() {
    g_clear_object(&camera);
}

optional<CamError> AravisBackend::startAcquisition() {
    size_t payload = arv_camera_get_payload(camera, &error);

    // Insert some buffers in the stream buffer pool
    for (int i = 0; i < stream_buffer_count; i++) {
        arv_stream_push_buffer(stream->m_stream, arv_buffer_new(payload, NULL));
    }

    ARV_RET_OPT(arv_camera_start_acquisition(camera, &error), error);
}

optional<CamError> AravisBackend::stopAcquisition() {
    ARV_RET_OPT(arv_camera_stop_acquisition(camera, &error), error);
}

optional<CamError> AravisBackend::setAcquisitionMode(AcquisitionMode mode) {
    ArvAcquisitionMode arvMode = acq_mode_map.at(mode);
    ARV_RET_OPT(arv_camera_set_acquisition_mode(camera, arvMode, &error), error);
}

optional<CamError> AravisBackend::setPixelFormat(PixelFormat format) {
    ArvPixelFormat arvFormat = pixel_format_map.at(format);
    ARV_RET_OPT(arv_camera_set_pixel_format(camera, arvFormat, &error), error);
}

optional<CamError> AravisBackend::setBinning(int dx, int dy) {
    ARV_RET_OPT(arv_camera_set_binning(camera, dx, dy, &error), error);
}

optional<CamError> AravisBackend::setGain(double gain) {
    ARV_RET_OPT(arv_camera_set_gain(camera, gain, &error), error);
}

optional<CamError> AravisBackend::setAutoExposure(bool setAuto) {
    ARV_RET_OPT(arv_camera_set_exposure_time_auto(
        camera,
        setAuto ? ARV_AUTO_CONTINUOUS : ARV_AUTO_OFF,
        &error
    ), error);
}

optional<CamError> AravisBackend::setExposureTime(double exposure_time_us) {
    ARV_RET_OPT(
        arv_camera_set_exposure_time(camera, exposure_time_us, &error),
        error);
}

optional<CamError> AravisBackend::setFrameRate(double framerate) {
    ARV_RET_OPT(arv_camera_set_frame_rate(camera, framerate, &error), error);
}

optional<CamError> AravisBackend::enableLensPower(bool enable) {
    ArvDevice *device = arv_camera_get_device(camera);
    GError *err = NULL;

    // Try direct V3_3Enable node first
    ArvGcNode *v33node = arv_device_get_feature(device, "V3_3Enable");
    if (v33node != NULL) {
        arv_device_set_boolean_feature_value(device, "V3_3Enable", enable, &err);
        ARV_CHECK_ERROR(err);
        return nullopt;
    }

    // Fallback: configure Line2 as output sourced from UserOutput1
    arv_device_set_string_feature_value(device, "LineSelector", "Line2", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_string_feature_value(device, "LineMode", "Output", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_string_feature_value(device, "LineSource", "UserOutput1", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_boolean_feature_value(device, "UserOutputValue", enable, &err);
    ARV_CHECK_ERROR(err);

    return nullopt;
}

optional<CamError> AravisBackend::setupLensSerial() {
    ArvDevice *device = arv_camera_get_device(camera);
    GError *err = NULL;

    // Select Line1, set as output, source from SerialPort0 Tx
    arv_device_set_string_feature_value(device, "LineSelector", "Line1", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_string_feature_value(device, "LineMode", "Output", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_string_feature_value(device, "LineSource", "SerialPort0_Tx", &err);
    ARV_CHECK_ERROR(err);

    // Set baud rate to 57600
    arv_device_set_string_feature_value(device, "SerialPortBaudRate", "Baud57600", &err);
    ARV_CHECK_ERROR(err);

    return nullopt;
}

optional<CamError> AravisBackend::setLensFocus(double voltage) {
    // Clamp voltage to safe range [24.0V, 70.0V]
    double safe_volts = std::max(24.0, std::min(voltage, 70.0));
    uint16_t raw = static_cast<uint16_t>((safe_volts - 24.0) * 1000.0);

    // Build 7-byte command packet
    uint8_t packet[7];
    packet[0] = 0x02;              // STX
    packet[1] = 0x37;              // Command: set focal power
    packet[2] = 0x00;              // Address
    packet[3] = 0x02;              // Payload length
    packet[4] = raw & 0xFF;        // Value low byte
    packet[5] = (raw >> 8) & 0xFF; // Value high byte

    // Checksum: sum of all preceding bytes mod 256
    uint8_t checksum = 0;
    for (int i = 0; i < 6; i++) {
        checksum += packet[i];
    }
    packet[6] = checksum;

    return writeSerialFileAccess(packet, sizeof(packet));
}

optional<CamError> AravisBackend::writeSerialFileAccess(const void* data, size_t length) {
    ArvDevice *device = arv_camera_get_device(camera);
    GError *err = NULL;

    // Select the serial port file
    arv_device_set_string_feature_value(device, "FileSelector", "SerialPort0", &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_string_feature_value(device, "FileOpenMode", "Write", &err);
    ARV_CHECK_ERROR(err);

    // Open
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Open", &err);
    ARV_CHECK_ERROR(err);

    arv_device_execute_command(device, "FileOperationExecute", &err);
    ARV_CHECK_ERROR(err);

    // Write data into the FileAccessBuffer register node
    ArvGc *genicam = arv_device_get_genicam(device);
    ArvGcNode *buffer_node = arv_gc_get_node(genicam, "FileAccessBuffer");
    if (buffer_node == NULL) {
        return CamError { .message = "FileAccessBuffer node not found" };
    }
    arv_gc_register_set(ARV_GC_REGISTER(buffer_node), data, length, &err);
    ARV_CHECK_ERROR(err);

    arv_device_set_integer_feature_value(device, "FileAccessLength", (gint64)length, &err);
    ARV_CHECK_ERROR(err);

    // Execute write
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Write", &err);
    ARV_CHECK_ERROR(err);

    arv_device_execute_command(device, "FileOperationExecute", &err);
    ARV_CHECK_ERROR(err);

    // Close
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Close", &err);
    ARV_CHECK_ERROR(err);

    arv_device_execute_command(device, "FileOperationExecute", &err);
    ARV_CHECK_ERROR(err);

    return nullopt;
}

shared_ptr<IStream> AravisBackend::getStream() {
    return stream;
}

}  // namespace camera
}  // namespace cynlr
