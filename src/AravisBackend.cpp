#include <memory>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>

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

std::vector<std::string> AravisBackend::listCameras() {
    arv_update_device_list();
    unsigned int n = arv_get_n_devices();
    std::vector<std::string> names;
    names.reserve(n);
    for (unsigned int i = 0; i < n; i++) {
        names.emplace_back(arv_get_device_id(i));
    }
    return names;
}

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
    if (serial_port_open) {
        ArvDevice *device = arv_camera_get_device(camera);
        GError *err = NULL;
        arv_device_set_string_feature_value(device, "FileSelector", "SerialPort0", &err);
        arv_device_set_string_feature_value(device, "FileOperationSelector", "Close", &err);
        arv_device_execute_command(device, "FileOperationExecute", &err);
        if (err) g_clear_error(&err);
    }
    g_clear_object(&camera);
}

optional<CamError> AravisBackend::startAcquisition() {
    size_t payload = arv_camera_get_payload(camera, &error);

    // Insert some buffers in the stream buffer pool
    for (uint32_t i = 0; i < stream_buffer_count; i++) {
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
    if (v33node == NULL) {
        return CamError { .message = "V3_3Enable feature not found" };
    }
    arv_device_set_boolean_feature_value(device, "V3_3Enable", enable, &err);
    ARV_CHECK_ERROR(err);
    return nullopt;
}

optional<CamError> AravisBackend::setupLensSerial(const char* baudRate) {
    ArvDevice *device = arv_camera_get_device(camera);
    GError *err = NULL;

    // Line0: Input (Rx) — LineSource not applicable for input lines
    arv_device_set_string_feature_value(device, "LineSelector", "Line0", &err);
    ARV_CHECK_ERROR(err);
    arv_device_set_string_feature_value(device, "LineMode", "Input", &err);
    ARV_CHECK_ERROR(err);

    // Line1: Output, source from SerialPort0 (Tx)
    arv_device_set_string_feature_value(device, "LineSelector", "Line1", &err);
    ARV_CHECK_ERROR(err);
    arv_device_set_string_feature_value(device, "LineMode", "Output", &err);
    ARV_CHECK_ERROR(err);
    arv_device_set_string_feature_value(device, "LineSource", "SerialPort0", &err);
    ARV_CHECK_ERROR(err);

    // SerialPort0 source from Line0
    arv_device_set_string_feature_value(device, "SerialPortSelector", "SerialPort0", &err);
    ARV_CHECK_ERROR(err);
    arv_device_set_string_feature_value(device, "SerialPortSource", "Line0", &err);
    ARV_CHECK_ERROR(err);

    // Set baud rate
    arv_device_set_string_feature_value(device, "SerialPortBaudRate", baudRate, &err);
    ARV_CHECK_ERROR(err);

    // Select SerialPort0 as the file for file access operations
    arv_device_set_string_feature_value(device, "FileSelector", "SerialPort0", &err);
    ARV_CHECK_ERROR(err);

    // Open the serial port for writing
    arv_device_set_string_feature_value(device, "FileOpenMode", "Write", &err);
    ARV_CHECK_ERROR(err);
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Open", &err);
    ARV_CHECK_ERROR(err);
    arv_device_execute_command(device, "FileOperationExecute", &err);
    ARV_CHECK_ERROR(err);
    printf("  [serial] SerialPort0 opened for Write\n");
    serial_port_open = true;

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

    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    printf("  [serial] write len=%zu data=", length);
    for (size_t i = 0; i < length; i++) printf("%02X", bytes[i]);
    printf("\n");

    // Ensure FileSelector points to SerialPort0 (may have changed since setupLensSerial)
    arv_device_set_string_feature_value(device, "FileSelector", "SerialPort0", &err);
    ARV_CHECK_ERROR(err);

    // FileAccessOffset = 0
    arv_device_set_integer_feature_value(device, "FileAccessOffset", 0, &err);
    ARV_CHECK_ERROR(err);

    // FileAccessLength
    arv_device_set_integer_feature_value(device, "FileAccessLength", (gint64)length, &err);
    ARV_CHECK_ERROR(err);
    printf("  [serial] FileAccessLength=%zu OK\n", length);

    // FileOperationSelector = "Write" (must be BEFORE writing FileAccessBuffer)
    arv_device_set_string_feature_value(device, "FileOperationSelector", "Write", &err);
    ARV_CHECK_ERROR(err);

    // FileAccessBuffer — write directly at physical address
    ArvGc *genicam = arv_device_get_genicam(device);
    ArvGcNode *buffer_node = arv_gc_get_node(genicam, "FileAccessBuffer");
    if (buffer_node == NULL) {
        return CamError { .message = "FileAccessBuffer node not found" };
    }

    GError *addr_err = NULL;
    guint64 reg_addr = arv_gc_register_get_address(ARV_GC_REGISTER(buffer_node), &addr_err);
    arv_device_write_memory(device, reg_addr, (guint32)length, const_cast<void*>(data), &err);
    ARV_CHECK_ERROR(err);

    printf("  [serial] FileAccessBuffer OK\n");

    // FileOperationExecute
    arv_device_execute_command(device, "FileOperationExecute", &err);
    ARV_CHECK_ERROR(err);
    gint64 result = arv_device_get_integer_feature_value(device, "FileOperationResult", &err);
    if (err != NULL) {
        g_clear_error(&err);
    } else {
        printf("  [serial] FileOperationExecute OK, result=%lld bytes\n", (long long)result);
    }

    return nullopt;
}

shared_ptr<IStream> AravisBackend::getStream() {
    return stream;
}

}  // namespace camera
}  // namespace cynlr
