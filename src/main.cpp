#include <hackrf_sweeper.h>
#include <libhackrf/hackrf.h>
#include <libusb-1.0/libusb.h>

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QVBoxLayout>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

static const int HACKRF_VENDOR_ID = 0x1d50;
static const int BIN_WIDTH = 1'000'000;

struct hackrf_gain_state {
    bool amp_enable;
    int vga_gain;
    int lna_gain;
};

class HackRFDevice {
   private:
    hackrf_device *device = nullptr;
    hackrf_gain_state gain_state;
    bool connected = false;
    std::mutex device_mutex;

   public:
    HackRFDevice() {
        gain_state = {
            .amp_enable = false,
            .vga_gain = 32,
            .lna_gain = 0,
        };
        connect();
    }

    void connect() {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (connected) {
            hackrf_close(device);
        }

        int result = hackrf_open(&device);

        if (result != HACKRF_SUCCESS) {
            std::cout << "HackRF device not connected\n";

            connected = false;
            device = nullptr;
        } else {
            connected = true;

            update_hackrf_gain_state();
        }
    }

    void setGainState(hackrf_gain_state state) {
        std::lock_guard<std::mutex> lock(device_mutex);

        gain_state = state;
        update_hackrf_gain_state();
    }

    void setAmpEnable(bool enable) {
        std::lock_guard<std::mutex> lock(device_mutex);

        gain_state.amp_enable = enable;
        update_hackrf_gain_state();
    }

    void setVGAGain(int gain) {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (gain % 2 != 0 || gain < 0 || gain > 62) {
            std::cerr << "Invalid VGA gain value: " << gain << std::endl;
            return;
        }

        gain_state.vga_gain = gain;
        update_hackrf_gain_state();
    }

    void setLNAGain(int gain) {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (gain % 8 != 0 || gain < 0 || gain > 40) {
            std::cerr << "Invalid LNA gain value: " << gain << std::endl;
            return;
        }

        gain_state.lna_gain = gain;
        update_hackrf_gain_state();
    }

    hackrf_gain_state getGainState() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return gain_state;
    }

    bool isConnected() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return connected;
    }

    hackrf_device *getDevice() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return device;
    }

    void update_hackrf_gain_state() {
        if (device == nullptr) {
            std::cerr << "HackRF device is not connected." << std::endl;
            return;
        }

        hackrf_set_amp_enable(device, gain_state.amp_enable);
        hackrf_set_vga_gain(device, gain_state.vga_gain);
        hackrf_set_lna_gain(device, gain_state.lna_gain);
    }

    int getTotalGain() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return gain_state.lna_gain + gain_state.vga_gain + gain_state.amp_enable * 14;
    }

    ~HackRFDevice() {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (device != nullptr) {
            hackrf_close(device);
        }
    }
};

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
    static libusb_device_handle *dev_handle = NULL;
    struct libusb_device_descriptor desc;

    (void)libusb_get_device_descriptor(dev, &desc);

    HackRFDevice *hackrf = static_cast<HackRFDevice*>(user_data);

    if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED || event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        hackrf->connect();
    } else {
        printf("Unhandled event %d\n", event);
    }

    return 0;
}

QFormLayout *getControlLayout(QWidget *window, HackRFDevice *device) {
    QFormLayout *slider_layout = new QFormLayout(window);

    QCheckBox *amp_enable_checkbox = new QCheckBox("AMP");
    amp_enable_checkbox->setChecked(device->getGainState().amp_enable);
    QObject::connect(amp_enable_checkbox, &QCheckBox::stateChanged, [device](int state) {
        device->setAmpEnable(state == Qt::Checked);
    });

    QLabel *lna_gain_label = new QLabel("LNA gain");
    QSlider *lna_gain_slider = new QSlider(Qt::Horizontal);
    lna_gain_slider->setRange(0, 5);  // 40 / 8 = 5
    lna_gain_slider->setValue(device->getGainState().lna_gain / 8);
    lna_gain_slider->setSingleStep(1);
    lna_gain_slider->setTickInterval(1);

    QObject::connect(lna_gain_slider, &QSlider::valueChanged, [device](int value) {
        int lna_gain = value * 8;
        device->setLNAGain(lna_gain);
    });

    QLabel *vga_gain_label = new QLabel("VGA gain");
    QSlider *vga_gain_slider = new QSlider(Qt::Horizontal);
    vga_gain_slider->setRange(0, 31);  // 62 / 2 = 31
    vga_gain_slider->setValue(device->getGainState().vga_gain / 2);
    vga_gain_slider->setSingleStep(1);
    vga_gain_slider->setTickInterval(1);

    QObject::connect(vga_gain_slider, &QSlider::valueChanged, [device](int value) {
        int vga_gain = value * 2;
        device->setVGAGain(vga_gain);
    });

    QLabel *total_gain_label = new QLabel("Total Gain:");
    QLineEdit *total_gain_field = new QLineEdit();
    total_gain_field->setReadOnly(true);

    auto updateTotalGain = [device, total_gain_field]() {
        int total_gain = device->getTotalGain();
        total_gain_field->setText(QString::number(total_gain) + " dB");
    };

    QObject::connect(amp_enable_checkbox, &QCheckBox::stateChanged, updateTotalGain);
    QObject::connect(lna_gain_slider, &QSlider::valueChanged, updateTotalGain);
    QObject::connect(vga_gain_slider, &QSlider::valueChanged, updateTotalGain);

    slider_layout->addRow(amp_enable_checkbox);
    slider_layout->addRow(lna_gain_label, lna_gain_slider);
    slider_layout->addRow(vga_gain_label, vga_gain_slider);
    slider_layout->addRow(total_gain_label, total_gain_field);

    updateTotalGain();

    return slider_layout;
}

int main(int argc, char **argv) {
    hackrf_init();

    HackRFDevice hackrf;

    hackrf_gain_state gain_state = {
        .amp_enable = false,
        .vga_gain = 32,
        .lna_gain = 0,
    };

    hackrf.setGainState(gain_state);

    libusb_hotplug_callback_handle callback_handle;

    libusb_init(nullptr);

    int rc = libusb_hotplug_register_callback(
        NULL, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0, HACKRF_VENDOR_ID, USB_BOARD_ID_HACKRF_ONE,
        LIBUSB_HOTPLUG_MATCH_ANY, hotplug_callback, &hackrf, &callback_handle);

    if (LIBUSB_SUCCESS != rc) {
        printf("Error creating a hotplug callback\n");
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }

    std::thread libusb_refresh_events([]() {
        while (true) {
            libusb_handle_events_completed(NULL, NULL);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });

    QApplication app(argc, argv);

    QWidget window;

    QVBoxLayout *layout = new QVBoxLayout(&window);
    QFormLayout *slider_layout = getControlLayout(nullptr, &hackrf);

    layout->addLayout(slider_layout);

    window.setLayout(layout);

    window.show();

    int exec = app.exec();

    libusb_refresh_events.detach();

    return exec;
}
