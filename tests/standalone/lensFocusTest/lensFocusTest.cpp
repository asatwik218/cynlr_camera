#include <opencv2/opencv.hpp>
#include "Camera.hpp"
#include "AravisBackend.hpp"
#include <cstdio>
#include <csignal>
#include <thread>
#include <chrono>
#include <atomic>

using namespace cynlr::camera;

static std::atomic<bool> g_running{true};

static void signalHandler(int) {
    g_running = false;
}

int main() {
    printf("Single Camera Lens Focus Test\n");

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    const char* cam_name = nullptr;  // auto-detect first available camera

    auto backend = AravisBackend::create(cam_name);
    if (!backend) {
        printf("Failed to create camera backend: %s\n", cam_name);
        return -1;
    }

    Camera cam(std::move(backend));

    // Configure camera
    printf("Configuring camera...\n");
    abortOnError(cam.stopAcquisition());
    abortOnError(cam.setPixelFormat(PixelFormat::MONO8));
    abortOnError(cam.setBinning(4, 4));
    abortOnError(cam.setFrameRate(24.0));
    abortOnError(cam.setAcquisitionMode(AcquisitionMode::ACQUISITION_MODE_CONTINUOUS));
    
    // Setup lens serial (also inits lens at 24V)
    printf("Setting up lens serial...\n");
    abortOnError(cam.setupLensSerial("Baud57600"));

    // Power-cycle 3.3V to reset lens hardware state
    printf("Power On 3.3V lens power...\n");
    abortOnError(cam.enableLensPower(true));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Focus starts at 24V (set by setupLensSerial init)
    double focus_voltage = 24.0;
    double focus_step = 0.5;

    // Start acquisition
    printf("Starting acquisition...\n");
    abortOnError(cam.startAcquisition());

    // Wait for camera to fill buffers
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    cv::namedWindow("Camera", cv::WINDOW_AUTOSIZE);

    printf("\n=== Controls ===\n");
    printf("  w : Focus up   (+step)\n");
    printf("  x : Focus down (-step)\n");
    printf("  e : Focus up   (+5.0V)\n");
    printf("  z : Focus down (-5.0V)\n");
    printf("  +/- : Change step size\n");
    printf("  s   : Save snapshot\n");
    printf("  q   : Quit\n");
    printf("  Focus range: 24.0V - 70.0V\n");
    printf("================\n\n");

    FrameBuffer frame{};
    FrameBuffer prev_frame{};
    bool has_prev = false;

    // Grab an initial frame so we always have a prev_frame to release
    abortOnError(cam.borrowNewestFrame(prev_frame));
    has_prev = true;

    while (g_running) {
        auto err = cam.borrowNewestFrame(frame);
        if (err.has_value()) continue;

        if (has_prev) cam.releaseFrame(prev_frame);

        cv::Mat mat(frame.height, frame.width, CV_8UC1, frame.data);
        cv::Mat display;
        cv::cvtColor(mat, display, cv::COLOR_GRAY2BGR);

        char info[128];
        snprintf(info, sizeof(info), "Focus: %.1fV  Step: %.1fV", focus_voltage, focus_step);
        cv::putText(display, info, cv::Point(10, 25),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 0), 2);

        cv::imshow("Camera", display);

        int key = cv::waitKeyEx(1);
        if (key == 'q' || key == 27) break;

        bool focus_changed = false;

        switch (key) {
            case 'w': case 'W':
                focus_voltage = std::min(focus_voltage + focus_step, 70.0);
                focus_changed = true;
                break;
            case 'x': case 'X':
                focus_voltage = std::max(focus_voltage - focus_step, 24.0);
                focus_changed = true;
                break;
            case 'e': case 'E':
                focus_voltage = std::min(focus_voltage + 5.0, 70.0);
                focus_changed = true;
                break;
            case 'z': case 'Z':
                focus_voltage = std::max(focus_voltage - 5.0, 24.0);
                focus_changed = true;
                break;
            case '+': case '=':
                focus_step = std::min(focus_step * 2.0, 5.0);
                printf("Step size: %.1fV\n", focus_step);
                break;
            case '-': case '_':
                focus_step = std::max(focus_step / 2.0, 0.1);
                printf("Step size: %.1fV\n", focus_step);
                break;
            case 's':
                cv::imwrite("snapshot.png", mat);
                printf("Snapshot saved (focus=%.1fV)\n", focus_voltage);
                break;
        }

        if (focus_changed) {
            printf("Focus: %.1fV\n", focus_voltage);
            auto ferr = cam.setLensFocus(focus_voltage);
            if (ferr.has_value())
                printf("  setLensFocus FAILED: %s\n", ferr->message);
        }
    }

    // Cleanup — runs on q/ESC exit AND on Ctrl+C
    printf("\nCleaning up...\n");
    if (has_prev) cam.releaseFrame(prev_frame);
    cam.stopAcquisition();
    cam.enableLensPower(false);
    cv::destroyAllWindows();
    printf("Done. Final focus was %.1fV\n", focus_voltage);
    return 0;
}
