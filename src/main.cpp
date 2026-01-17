#include <libusb-1.0/libusb.h>

#include <QApplication>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

#include "hackrf_controller.hpp"
#include "main_window.hpp"

using namespace std::chrono_literals;

constexpr int HACKRF_VENDOR_ID = 0x1d50;
constexpr auto HOTPLUG_DELAY = 200ms;

int hotplug_callback(struct libusb_context* /*ctx*/, struct libusb_device* /*dev*/,
                     libusb_hotplug_event /*event*/, void* user_data) {
    auto* controller = static_cast<HackRFController*>(user_data);

    std::this_thread::sleep_for(HOTPLUG_DELAY);

    controller->connect_device();
    controller->start_sweep();
    return 0;
}

int main(int argc, char* argv[]) {
    hackrf_init();

    HackRFController controller;

    if (controller.connect_device()) {
        controller.start_sweep();

        HackRFGainState gain_state{.amp_enable = false, .vga_gain = 24, .lna_gain = 0};
        controller.set_gain_state(gain_state);
    }

    libusb_hotplug_callback_handle callback_handle{};
    libusb_context* libusb_ctx = nullptr;
    int rc = libusb_init(&libusb_ctx);

    if (rc != LIBUSB_SUCCESS) {
        std::cerr << "Failed to initialize libusb: " << rc << "\n";
    } else {
        rc = libusb_hotplug_register_callback(
            libusb_ctx,
            LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED,
            0, HACKRF_VENDOR_ID, USB_BOARD_ID_HACKRF_ONE, LIBUSB_HOTPLUG_MATCH_ANY,
            hotplug_callback, &controller, &callback_handle);

        if (rc != LIBUSB_SUCCESS) {
            std::cerr << "Error creating a hotplug callback: " << rc << "\n";
            libusb_hotplug_deregister_callback(libusb_ctx, callback_handle);
            libusb_exit(libusb_ctx);
            libusb_ctx = nullptr;
        }
    }

    std::atomic_bool libusb_running{libusb_ctx != nullptr};
    std::thread libusb_refresh_events;
    if (libusb_ctx) {
        libusb_refresh_events = std::thread([&libusb_running, libusb_ctx]() {
            while (libusb_running.load(std::memory_order_relaxed)) {
                libusb_handle_events_completed(libusb_ctx, nullptr);
            }
        });
    }

    QApplication app(argc, argv);
    MainWindow main_window(&controller);
    main_window.showMaximized();

    int ret = app.exec();

    if (libusb_ctx) {
        libusb_running.store(false, std::memory_order_relaxed);
        if (libusb_refresh_events.joinable()) {
            libusb_refresh_events.join();
        }
        libusb_hotplug_deregister_callback(libusb_ctx, callback_handle);
        libusb_exit(libusb_ctx);
    }
    if (controller.is_connected()) {
        controller.stop_sweep();
    }

    hackrf_exit();

    return ret;
}
