#include <opencv2/opencv.hpp>

#include "Camera.hpp"
#include "AravisBackend.hpp"

using namespace cynlr::camera;

int main(int argc, char *argv[]) {
    printf("Camera Grabber Test\n");

    const char *cam_name = nullptr;
    auto aravisBackend = AravisBackend::create(cam_name);
    if (aravisBackend == nullptr) {
        printf("Aravis backed camera could not be created\n");
        return -1;
    }
    Camera cam(std::move(aravisBackend));

    printf("Configuring camera...\n");

    abortOnError(cam.stopAcquisition());
    abortOnError(cam.setPixelFormat(PixelFormat::MONO8));
    abortOnError(cam.setBinning(4, 4));
    abortOnError(cam.setFrameRate(60.0));
    abortOnError(cam.setAcquisitionMode(AcquisitionMode::ACQUISITION_MODE_CONTINUOUS));
    
    printf("Starting acquisition...\n");
    abortOnError(cam.startAcquisition());

    FrameBuffer curr_frame;
    FrameBuffer prev_frame;

    abortOnError(cam.borrowNewestFrame(prev_frame));

    /* Test newest frame */
    while(1) {
        abortOnError(cam.borrowNewestFrame(curr_frame));
        cv::Mat cv_frame(
            curr_frame.height,
            curr_frame.width,
            CV_8UC1,
            curr_frame.data
        );

        cv::imshow("Camera Grabber Test (newest frame)", cv_frame);
        if(cv::waitKey(1) == 27) {
            break;
        }

        cam.releaseFrame(prev_frame);
        prev_frame = curr_frame;
    }

    /* Test oldest frame */
    while(1) {
        abortOnError(cam.borrowOldestFrame(curr_frame));

        cv::Mat cv_frame(
            curr_frame.height,
            curr_frame.width,
            CV_8UC1,
            curr_frame.data
        );

        cv::imshow("Camera Grabber Test (oldest frame)", cv_frame);
        if(cv::waitKey(1) == 27) {
            break;
        }

        cam.releaseFrame(prev_frame);
        prev_frame = curr_frame;
    }  

    /* Test next new frame */
    while(1) {
        abortOnError(cam.borrowNextNewFrame(curr_frame));

        cv::Mat cv_frame(
            curr_frame.height,
            curr_frame.width,
            CV_8UC1,
            curr_frame.data
        );

        cv::imshow("Camera Grabber Test (next new frame)", cv_frame);
        if(cv::waitKey(1) == 27) {
            break;
        }

        cam.releaseFrame(prev_frame);
        prev_frame = curr_frame;
    }

    cam.releaseFrame(curr_frame);
    cam.releaseFrame(prev_frame);

    return 0;
}