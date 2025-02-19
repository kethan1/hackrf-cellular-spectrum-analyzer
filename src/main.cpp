#include <libusb-1.0/libusb.h>

#include <QApplication>
#include <chrono>
#include <thread>
#include <iostream>
#include <thread>
#include <chrono>
#include <thread>

#include "hackrf_controller.hpp"
#include "main_window.hpp"

using namespace std::chrono_literals;

const int HACKRF_VENDOR_ID = 0x1d50;

int hotplug_callback(struct libusb_context * /*ctx*/, struct libusb_device * /*dev*/,
                     libusb_hotplug_event /*event*/, void *user_data) {
    HackRF_Controller *controller = static_cast<HackRF_Controller *>(user_data);

    std::this_thread::sleep_for(2s);

    controller->connect_device();
    return 0;
}

int main(int argc, char *argv[]) {
    hackrf_init();

    HackRF_Controller controller;
    controller.connect_device();

    hackrf_gain_state gain_state = {false, 0, 24};
    controller.set_gain_state(gain_state);

    libusb_hotplug_callback_handle callback_handle;
    libusb_init(nullptr);

    int rc = libusb_hotplug_register_callback(
        nullptr,
        LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
        0, HACKRF_VENDOR_ID, USB_BOARD_ID_HACKRF_ONE, LIBUSB_HOTPLUG_MATCH_ANY,
        hotplug_callback, &controller, &callback_handle);

    if (rc != LIBUSB_SUCCESS) {
        std::cerr << "Error creating a hotplug callback\n";
        libusb_exit(nullptr);
        return EXIT_FAILURE;
    }

    std::thread libusb_refresh_events([]() {
        while (true) {
            libusb_handle_events_completed(nullptr, nullptr);
            std::this_thread::sleep_for(100ms);
        }
    });

    controller.start_sweep();

    QApplication app(argc, argv);
    MainWindow main_window(&controller);
    main_window.showMaximized();

    int ret = app.exec();

    libusb_refresh_events.detach();
    controller.stop_sweep();

    return ret;
}
