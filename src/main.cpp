#include <libhackrf/hackrf.h>
extern "C" {
#include <hackrf_sweeper.h>
}
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
#include <vector>

#include "qcustomplot.h"

const int HACKRF_VENDOR_ID = 0x1d50;
const int FFT_BIN_WIDTH = 10'000;
const uint16_t SWEEP_FREQ_MIN_MHZ = 700;
const uint16_t SWEEP_FREQ_MAX_MHZ = 3'700;
QCustomPlot *customPlot;

int hackrf_sweep_fft_callback(void *sweep_state, uint64_t frequency, hackrf_transfer *transfer) {
    hackrf_sweep_state_t *ctx = static_cast<hackrf_sweep_state_t *>(sweep_state);

    hackrf_sweep_fft_ctx_t fft = ctx->fft;

    std::vector<double> power_spectrum(fft.size / 2);
    for (int i = 0; i < fft.size / 2; ++i) {
        float real = fft.fftw_out[i][0];
        float imag = fft.fftw_out[i][1];
        power_spectrum[i] = real * real + imag * imag;
        std::cout << real * real + imag * imag << '\n';
    }

    std::vector<double> frequencies(fft.size / 2);
    for (int i = 0; i < fft.size / 2; ++i) {
        frequencies[i] = i * fft.bin_width;
    }

    QMetaObject::invokeMethod(customPlot, [frequencies, power_spectrum]() {
        customPlot->graph(0)->setData(QVector<double>(frequencies.begin(), frequencies.end()),
                                      QVector<double>(power_spectrum.begin(), power_spectrum.end()));
        customPlot->rescaleAxes();
        customPlot->replot();
    });

    return 0;
}

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
    hackrf_sweep_state_t *sweep_state = nullptr;
    bool sweeping = false;

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

            result = hackrf_set_sample_rate_manual(device, DEFAULT_SAMPLE_RATE_HZ, 1);
            if (result != HACKRF_SUCCESS) {
                std::cerr << "Failed to set sample rate" << std::endl;
            }

            result = hackrf_set_baseband_filter_bandwidth(device, DEFAULT_BASEBAND_FILTER_BANDWIDTH);
            if (result != HACKRF_SUCCESS) {
                std::cerr << "Failed to set baseband filter bandwidth" << '\n';
            }

            update_hackrf_gain_state();

            if (sweep_state != nullptr) {
                delete sweep_state;
            }
            sweep_state = new hackrf_sweep_state_t();

            result = hackrf_sweep_easy_init(device, sweep_state);
            if (result != HACKRF_SUCCESS) {
                std::cerr << "Failed to initialize sweep state" << '\n';
            }

            hackrf_sweep_set_output(sweep_state, HACKRF_SWEEP_OUTPUT_MODE_IFFT, HACKRF_SWEEP_OUTPUT_TYPE_NOP, nullptr);

            uint16_t freq_range[] = {SWEEP_FREQ_MIN_MHZ, SWEEP_FREQ_MAX_MHZ};
            result = hackrf_sweep_set_range(sweep_state, freq_range, 1);
            if (result != HACKRF_SUCCESS) {
                std::cerr << "Failed to set sweep range " << result << '\n';
            }

            result = hackrf_sweep_setup_fft(sweep_state, FFTW_MEASURE, FFT_BIN_WIDTH);
            if (result != HACKRF_SUCCESS) {
                std::cerr << "Failed to setup FFT\n";
            }
        }
    }

    void start_sweep() {
        hackrf_sweep_set_fft_rx_callback(sweep_state, hackrf_sweep_fft_callback);

        int result = hackrf_sweep_start(sweep_state, 0);  // 0 = infinite sweep
        if (result != HACKRF_SUCCESS) {
            std::cerr << "Failed to start sweep " << result << '\n';
        }

        sweeping = true;
    }

    void stop_sweep() {
        hackrf_sweep_stop(sweep_state);

        sweeping = false;
    }

    void set_gain_state(hackrf_gain_state state) {
        std::lock_guard<std::mutex> lock(device_mutex);

        gain_state = state;
        update_hackrf_gain_state();
    }

    void set_amp_enable(bool enable) {
        std::lock_guard<std::mutex> lock(device_mutex);

        gain_state.amp_enable = enable;
        update_hackrf_gain_state();
    }

    void set_vga_gain(int gain) {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (gain % 2 != 0 || gain < 0 || gain > 62) {
            std::cerr << "Invalid VGA gain value: " << gain << '\n';
            return;
        }

        gain_state.vga_gain = gain;
        update_hackrf_gain_state();
    }

    void set_lna_gain(int gain) {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (gain % 8 != 0 || gain < 0 || gain > 40) {
            std::cerr << "Invalid LNA gain value: " << gain << '\n';
            return;
        }

        gain_state.lna_gain = gain;
        update_hackrf_gain_state();
    }

    hackrf_gain_state get_gain_state() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return gain_state;
    }

    void update_hackrf_gain_state() {
        if (device == nullptr) {
            std::cerr << "HackRF device is not connected." << std::endl;
            return;
        }

        bool prev_sweeping = sweeping;

        if (sweeping) {
            stop_sweep();
        }

        hackrf_set_amp_enable(device, gain_state.amp_enable);
        hackrf_set_vga_gain(device, gain_state.vga_gain);
        hackrf_set_lna_gain(device, gain_state.lna_gain);

        if (prev_sweeping) {
            start_sweep();
        }
    }

    bool is_connected() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return connected;
    }

    hackrf_device *get_device() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return device;
    }

    int get_total_gain() {
        std::lock_guard<std::mutex> lock(device_mutex);

        return gain_state.lna_gain + gain_state.vga_gain + gain_state.amp_enable * 14;
    }

    ~HackRFDevice() {
        std::lock_guard<std::mutex> lock(device_mutex);

        if (device != nullptr) {
            stop_sweep();
            hackrf_close(device);
        }
    }
};

int hotplug_callback(struct libusb_context *ctx, struct libusb_device *dev,
                     libusb_hotplug_event event, void *user_data) {
    static libusb_device_handle *dev_handle = NULL;
    struct libusb_device_descriptor desc;

    (void)libusb_get_device_descriptor(dev, &desc);

    HackRFDevice *hackrf = static_cast<HackRFDevice *>(user_data);

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
    amp_enable_checkbox->setChecked(device->get_gain_state().amp_enable);
    QObject::connect(amp_enable_checkbox, &QCheckBox::stateChanged, [device](int state) {
        device->set_amp_enable(state == Qt::Checked);
    });

    QLabel *lna_gain_label = new QLabel("LNA gain");
    QSlider *lna_gain_slider = new QSlider(Qt::Horizontal);
    lna_gain_slider->setRange(0, 5);  // 40 / 8 = 5
    lna_gain_slider->setValue(device->get_gain_state().lna_gain / 8);
    lna_gain_slider->setSingleStep(1);
    lna_gain_slider->setTickInterval(1);

    QObject::connect(lna_gain_slider, &QSlider::valueChanged, [device](int value) {
        int lna_gain = value * 8;
        device->set_lna_gain(lna_gain);
    });

    QLabel *vga_gain_label = new QLabel("VGA gain");
    QSlider *vga_gain_slider = new QSlider(Qt::Horizontal);
    vga_gain_slider->setRange(0, 31);  // 62 / 2 = 31
    vga_gain_slider->setValue(device->get_gain_state().vga_gain / 2);
    vga_gain_slider->setSingleStep(1);
    vga_gain_slider->setTickInterval(1);

    QObject::connect(vga_gain_slider, &QSlider::valueChanged, [device](int value) {
        int vga_gain = value * 2;
        device->set_vga_gain(vga_gain);
    });

    QLabel *total_gain_label = new QLabel("Total Gain:");
    QLineEdit *total_gain_field = new QLineEdit();
    total_gain_field->setReadOnly(true);

    auto updateTotalGain = [device, total_gain_field]() {
        int total_gain = device->get_total_gain();
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

    hackrf.set_gain_state(gain_state);

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

    QWidget *window = new QWidget();

    customPlot = new QCustomPlot();

    customPlot->addGraph();
    customPlot->xAxis->setLabel("Frequency (Hz)");
    customPlot->yAxis->setLabel("Power");
    customPlot->yAxis->setScaleType(QCPAxis::stLogarithmic);

    hackrf.start_sweep();

    QVBoxLayout *layout = new QVBoxLayout(window);
    QFormLayout *slider_layout = getControlLayout(nullptr, &hackrf);

    layout->addLayout(slider_layout);
    layout->addWidget(customPlot);

    window->setLayout(layout);

    window->show();

    int exec = app.exec();

    libusb_refresh_events.detach();

    return exec;
}
