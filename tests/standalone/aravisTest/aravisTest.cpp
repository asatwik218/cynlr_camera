#include <iostream>

#include <opencv2/opencv.hpp>
extern "C" {
    #include <arv.h>
}

#define ARV_CHECK(expr, error)                           \
    do {                                                 \
        (expr);                                          \
        if (error != NULL) {                             \
            fprintf(stderr,                              \
                    "Error: %s failed (%s) at %s:%d\n",  \
                    #expr, error->message, __FILE__, __LINE__); \
            abort();                                   \
        }                                                \
    } while (0)

int main() {
    std::cout << "Camera Grabber Test" << std::endl;

    // Initialize camera
    ArvCamera *camera;
    ArvStream *stream;
    size_t payload;
    GError *error = NULL;

    arv_update_device_list();

    printf("Number of cameras found: %d\n", arv_get_n_devices());
    camera = arv_camera_new(NULL, &error);

    if (error != NULL) {                             
        fprintf(stderr,                              
                "Error: arv_camera_new() failed (%s) at %s:%d\n",
                error->message, __FILE__, __LINE__); 
    }
    if (!ARV_IS_CAMERA(camera)) {
        printf("Camera NOT found !\n");
        return -1;
    }

    printf ("Found camera '%s'\n", arv_camera_get_model_name (camera, NULL));

    ARV_CHECK(arv_camera_stop_acquisition(camera, &error), error);
    ARV_CHECK(arv_camera_set_pixel_format(camera, ARV_PIXEL_FORMAT_MONO_8, &error), error);
    ARV_CHECK(arv_camera_set_region(camera, 612, 512, 1024, 1024, &error), error);
    ARV_CHECK(arv_camera_set_frame_rate(camera, 60.0, &error), error);
    ARV_CHECK(arv_camera_set_exposure_time_auto(camera, ARV_AUTO_CONTINUOUS, &error), error);
    ARV_CHECK(arv_camera_set_acquisition_mode(camera, ARV_ACQUISITION_MODE_CONTINUOUS, &error), error);
    printf("Pixel format: %s\n", arv_camera_get_pixel_format_as_string(camera, &error));

    stream = arv_camera_create_stream(camera, NULL, NULL, &error);

    if (!ARV_IS_STREAM(stream)) {
        printf("Stream not created\n");
        return -1;
    }

    payload = arv_camera_get_payload(camera, &error);

    // Insert some buffers in the stream buffer pool
    for (int i = 0; i < 2; i++) {
        arv_stream_push_buffer(stream, arv_buffer_new(payload, NULL));
    }

    ARV_CHECK(arv_camera_start_acquisition(camera, &error), error);

    ArvBuffer *prev_buffer = arv_stream_pop_buffer(stream);
    ArvBuffer *curr_buffer = NULL;

    while(1) {
        curr_buffer = arv_stream_pop_buffer(stream);

        if (ARV_IS_BUFFER (curr_buffer)) {
            /* Display some informations about the retrieved buffer */

            int width = arv_buffer_get_image_width (curr_buffer);
            int height = arv_buffer_get_image_height (curr_buffer);

            printf ("Acquired %dx%d buffer\n",
                width,
                height);

            size_t size = 0;
            void *arv_buffer = const_cast<void*>(arv_buffer_get_data(curr_buffer, &size));

            cv::Mat frame(
                height,
                width,
                CV_8UC1,
                arv_buffer
            );

            cv::imshow("Camera Grabber Test", frame);
            if(cv::waitKey(1) == 27) {
                break;
            }

            arv_stream_push_buffer(stream, prev_buffer);
            prev_buffer = curr_buffer;
        }
    }

    ARV_CHECK(arv_camera_stop_acquisition(camera, &error), error);

    g_clear_object (&curr_buffer);
    g_clear_object (&prev_buffer);
    g_clear_object (&camera);
    g_clear_object(&stream);

    return 0;
}