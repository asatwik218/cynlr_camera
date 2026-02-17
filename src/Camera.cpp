#include "Camera.hpp"

using namespace std;
using namespace cynlr::camera;

optional<CamError> Camera::startAcquisition() {
    return m_backend->startAcquisition();
}

optional<CamError> Camera::stopAcquisition() {
    return m_backend->stopAcquisition();
}

optional<CamError> Camera::setAcquisitionMode(AcquisitionMode mode) {
    return m_backend->setAcquisitionMode(mode);
}

optional<CamError> Camera::setPixelFormat(PixelFormat format) {
    return m_backend->setPixelFormat(format);
}

optional<CamError> Camera::setBinning(int dx, int dy) {
    return m_backend->setBinning(dx, dy);
}

optional<CamError> Camera::setGain(double gain) {
    return m_backend->setGain(gain);
}

optional<CamError> Camera::setAutoExposure(bool setAuto) {
    return m_backend->setAutoExposure(setAuto);
}

optional<CamError> Camera::setExposureTime(double exposure_time_us) {
    return m_backend->setExposureTime(exposure_time_us);
}

optional<CamError> Camera::setFrameRate(double framerate) {
    return m_backend->setFrameRate(framerate);
}

optional<CamError> Camera::enableLensPower(bool enable) {
    return m_backend->enableLensPower(enable);
}

optional<CamError> Camera::setupLensSerial(
    const char* line, const char* source, const char* baudRate) {
    return m_backend->setupLensSerial(line, source, baudRate);
}

optional<CamError> Camera::setLensFocus(double voltage) {
    return m_backend->setLensFocus(voltage);
}

optional<StreamError> Camera::borrowOldestFrame(FrameBuffer &frame) {
    return m_backend->getStream()->borrowOldestFrame(frame);
}

optional<StreamError> Camera::borrowNewestFrame(FrameBuffer &frame) {
    return m_backend->getStream()->borrowNewestFrame(frame);
}

optional<StreamError> Camera::borrowNextNewFrame(FrameBuffer &frame) {
    return m_backend->getStream()->borrowNextNewFrame(frame);
}

void Camera::releaseFrame(FrameBuffer &frame) {
    m_backend->getStream()->releaseFrame(frame);
}