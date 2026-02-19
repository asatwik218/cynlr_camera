# cynlr_camera

C++ camera library for GenICam/GigE/USB3Vision cameras, built on the [Aravis](https://aravisproject.github.io/aravis/) SDK.

## Requirements

- CMake 3.15+
- Conan 2
- MSVC (Windows, Release mode)

Dependencies managed by Conan: `aravis 0.8.33`, `opencv 4.12.0`

## Building

From the project root:

```bash
mkdir build && cd build

# Install dependencies (first time, or when conanfile.py changes)
conan install .. -s build_type=Release -s compiler="msvc" -s arch=x86_64 --build=missing --output-folder=.

# Configure
cmake .. -DCMAKE_TOOLCHAIN_FILE="build/conan_toolchain.cmake"

# Build
cmake --build . --config Release
```

> **Note:** The Conan build type and CMake config must match (`Release`/`Release`). Debug mode is not fully supported.

The library CMake target is `cynlr::camera`. Link against it with:

```cmake
target_link_libraries(your_target PRIVATE cynlr::camera)
```

---

## Usage

### 1. List available cameras

Before creating a camera, enumerate what is connected:

```cpp
#include "AravisBackend.hpp"

auto cameras = cynlr::camera::AravisBackend::listCameras();
for (const auto& name : cameras) {
    printf("  %s\n", name.c_str());
}
```

Returns a `std::vector<std::string>` of Aravis device IDs (e.g. `"FLIR-17412345"`). Pass one of these to `AravisBackend::create`, or pass `nullptr` to auto-select the first available camera.

---

### 2. Create a camera

```cpp
#include "AravisBackend.hpp"
#include "Camera.hpp"

using namespace cynlr::camera;

// Auto-detect first camera
auto backend = AravisBackend::create(nullptr);

// Or open a specific camera by name
auto backend = AravisBackend::create("FLIR-17412345");

if (!backend) {
    printf("Failed to open camera\n");
    return -1;
}

Camera cam(std::move(backend));
```

`AravisBackend::create` accepts an optional second argument for the stream buffer count (default: 10):

```cpp
auto backend = AravisBackend::create(nullptr, 5);
```

---

### 3. Configure and acquire frames

```cpp
cam.stopAcquisition();                                              // ensure stopped before configuring
cam.setPixelFormat(PixelFormat::MONO8);
cam.setBinning(1, 1);
cam.setFrameRate(30.0);
cam.setExposureTime(10000.0);                                       // microseconds
cam.setGain(0.0);
cam.setAcquisitionMode(AcquisitionMode::ACQUISITION_MODE_CONTINUOUS);
cam.startAcquisition();
```

---

### 4. Grab frames

Borrowed frames **must** be released back to the stream after use. There are three borrow modes:

```cpp
FrameBuffer frame;

// Newest available frame (discards older queued frames)
cam.borrowNewestFrame(frame);

// Oldest available frame (FIFO order)
cam.borrowOldestFrame(frame);

// Block until a brand-new frame arrives (discards all queued frames first)
cam.borrowNextNewFrame(frame);

// --- use frame ---
// frame.data    : raw pixel pointer
// frame.width   : image width in pixels
// frame.height  : image height in pixels
// frame.channels: number of channels

// Release back to the pool — required for every borrowed frame
cam.releaseFrame(frame);
```

#### Example: live display with OpenCV

```cpp
FrameBuffer frame;
while (running) {
    if (cam.borrowNewestFrame(frame)) continue;  // skip on error

    cv::Mat mat(frame.height, frame.width, CV_8UC1, frame.data);
    cv::imshow("Camera", mat);
    cam.releaseFrame(frame);

    if (cv::waitKey(1) == 27) break;
}
```

---

### 5. Lens control (liquid lens)

The lens is controlled over a GenICam serial port (SerialPort0 on Line0/Line1). Voltage range is **24.0 V – 70.0 V**.

**Initialization order is critical**: `setupLensSerial` must be called **before** `enableLensPower`.

```cpp
// 1. Configure serial port and send 24V init packet
cam.setupLensSerial("Baud57600");

// 2. Enable 3.3V lens power
cam.enableLensPower(true);
std::this_thread::sleep_for(std::chrono::milliseconds(500));  // allow lens to power up

// 3. Set focus voltage (24.0–70.0 V)
cam.setLensFocus(35.0);
```

To power-cycle the lens at startup (resets hardware state):

```cpp
cam.setupLensSerial("Baud57600");
cam.enableLensPower(false);
std::this_thread::sleep_for(std::chrono::milliseconds(500));
cam.enableLensPower(true);
std::this_thread::sleep_for(std::chrono::milliseconds(500));
```

The serial port is closed automatically when the `Camera` object is destroyed.

---

### 6. Error handling

All methods return `std::optional<CamError>` or `std::optional<StreamError>`. `std::nullopt` means success; a value means failure.

```cpp
if (auto err = cam.setPixelFormat(PixelFormat::MONO8)) {
    printf("Error: %s\n", err->message);
}
```

For quick prototyping, use `abortOnError` (defined in `Camera.hpp`) to abort on any error:

```cpp
abortOnError(cam.startAcquisition());
abortOnError(cam.borrowNextNewFrame(frame));
```

---

## API Reference

### `AravisBackend` (static methods)

| Method | Description |
|--------|-------------|
| `AravisBackend::create(name, buffers=10)` | Open a camera by Aravis device ID. Pass `nullptr` to auto-detect. |
| `AravisBackend::listCameras()` | Scan for connected cameras. Returns `std::vector<std::string>` of device IDs. |

### `Camera`

| Method | Description |
|--------|-------------|
| `startAcquisition()` | Start frame capture. |
| `stopAcquisition()` | Stop frame capture. |
| `setAcquisitionMode(mode)` | `ACQUISITION_MODE_CONTINUOUS`, `SINGLE_FRAME`, or `MULTI_FRAME`. |
| `setPixelFormat(format)` | `MONO8`, `MONO10`, `MONO12`, `MONO14`, `MONO16`. |
| `setBinning(dx, dy)` | Set horizontal and vertical binning. |
| `setGain(gain)` | Set sensor gain (dB). |
| `setAutoExposure(enable)` | Enable or disable auto exposure. |
| `setExposureTime(us)` | Set exposure time in microseconds. |
| `setFrameRate(fps)` | Set target frame rate. |
| `borrowOldestFrame(frame)` | Borrow oldest queued frame. |
| `borrowNewestFrame(frame)` | Borrow newest queued frame, discarding older ones. |
| `borrowNextNewFrame(frame)` | Block until a new frame arrives. |
| `releaseFrame(frame)` | Return a borrowed frame to the pool. Must be called for every borrow. |
| `setupLensSerial(baudRate)` | Configure serial port for lens control. Must be called before `enableLensPower`. |
| `enableLensPower(enable)` | Control 3.3V lens power supply. |
| `setLensFocus(voltage)` | Set lens focus voltage (24.0–70.0 V). |

---

## Architecture

```
Camera  (public API — Camera.hpp)
  └── ICameraBackend  (abstract interface — CameraBackend.hpp)
        └── AravisBackend  (Aravis/GenICam implementation — AravisBackend.hpp)
              └── AravisStream  (frame buffer management — AravisStream.hpp)
```

The `Camera` class is a thin facade that forwards all calls to an `ICameraBackend`. The only concrete backend is `AravisBackend`, which wraps the Aravis C library. This pattern allows alternative backends (e.g. simulation, other SDKs) to be swapped in without changing consumer code.
