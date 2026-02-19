# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses **Conan 2** for dependency management and **CMake 3.15+** for building. Dependencies: `aravis 0.8.33` (GenICam camera SDK) and `opencv 4.12.0`.

**Always use Release mode** — Debug mode is not fully supported yet.

### Windows Build

From a `build/` directory inside the project root:

```bash
# Install dependencies (first time or when conanfile.py changes)
conan install .. -s build_type=Release -s compiler="msvc" -s arch=x86_64 --build=missing --output-folder=.

# Configure
cmake .. -DCMAKE_TOOLCHAIN_FILE="build/conan_toolchain.cmake"

# Build
cmake --build . --config Release
```

**Important:** The Conan build type and CMake config must match (`Release`/`Release` or `Debug`/`Debug`).

### Publishing via Conan (CI)

The CI uses `conan create` with profiles from the separate `cynlr_conan` repository and uploads to a private Conan remote at `http://74.225.34.207:9300`.

## Testing

Tests in `tests/` require physical camera hardware — they are **not automated**. Two test executables are built:

- `aravisTest` — tests the raw Aravis API directly
- `cynlrCamTest` — tests the `cynlr_camera` library public API

Run them manually after connecting a camera. They display frames via OpenCV windows; press ESC to exit.

## Architecture

The library follows a **Strategy pattern** with a clean separation between the public API and backend implementations:

```
Camera (public API)
  └── ICameraBackend (abstract interface)
        └── AravisBackend (Aravis/GenICam implementation)
              └── AravisStream (implements IStream for frame acquisition)
```

- **`Camera`** (`include/Camera.hpp`, `src/Camera.cpp`) — Thin facade; forwards all calls to `m_backend`. This is what consumers use.
- **`ICameraBackend`** (`include/CameraBackend.hpp`) — Abstract interface; allows swapping camera implementations.
- **`AravisBackend`** (`include/AravisBackend.hpp`, `src/AravisBackend.cpp`) — Sole concrete implementation. Handles camera configuration (pixel format, binning, gain, exposure, frame rate, acquisition mode) and lens control (power, serial setup, focus voltage via 7-byte command packets with checksum, range 24–70V).
- **`AravisStream`** (`include/AravisStream.hpp`, `src/AravisStream.cpp`) — Frame buffer management wrapping Aravis stream. Provides three access modes: `borrowOldestFrame`, `borrowNewestFrame`, `borrowNextNewFrame` (blocking).
- **`Frame.hpp`** — `FrameBuffer` struct with `width`, `height`, and pixel data pointer.
- **`Constants.hpp`** — `AcquisitionMode` and `PixelFormat` enums mapping to Aravis constants.
- **`Error.hpp`** — `CamError` and `StreamError` types.

### Error Handling Convention

All operations return `std::optional<ErrorType>` — `std::nullopt` means success, a value means error. Example:

```cpp
if (auto err = cam.setPixelFormat(PixelFormat::MONO8)) {
    // handle error
}
```

### Factory and Usage Pattern

```cpp
auto backend = AravisBackend::create(camera_name, buffer_count);
cynlr::camera::Camera cam(std::move(backend));
cam.startAcquisition();

cynlr::camera::FrameBuffer frame;
cam.borrowNextNewFrame(frame);
// use frame...
cam.releaseFrame(frame);
```

Library target is `cynlr::camera` (CMake alias). Namespace is `cynlr::camera`.
