#include "hackrf_controller.hpp"

#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

static HackRF_Controller *global_controller = nullptr;

extern "C" {
    int hackrf_sweep_fft_callback(void *sweep_state, uint64_t current_freq, hackrf_transfer * /*transfer*/) {
        if (global_controller) {
            hackrf_sweep_state *state = static_cast<hackrf_sweep_state *>(sweep_state);

            global_controller->handle_fft(current_freq, state->fft.size, state->fft.bin_width, state->fft.pwr);
        }
        return 0;
    }
}

HackRF_Controller::HackRF_Controller()
    :device(nullptr), sweep_state(nullptr), connected(false), sweeping(false) {
    gain_state = {false, 32, 0};
    global_controller = this;
}

HackRF_Controller::~HackRF_Controller() {
    std::lock_guard<std::mutex> lock(device_mutex);
    if (device) {
        if (sweeping && sweep_state) {
            stop_sweep();
        }
        hackrf_close(device);
    }
    if (sweep_state) {
        delete sweep_state;
        sweep_state = nullptr;
    }
}

bool HackRF_Controller::connect_device() {
    std::lock_guard<std::mutex> lock(device_mutex);

    if (connected && device) {
        hackrf_close(device);
        device = nullptr;
        connected = false;
    }

    int result = hackrf_open(&device);

    if (result != HACKRF_SUCCESS) {
        std::cerr << "HackRF device not connected: " << result << "\n";
        connected = false;
        device = nullptr;
        return false;
    }
    connected = true;

    result = hackrf_set_sample_rate_manual(device, 5e6, 1);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sample rate\n";
    }

    result = hackrf_set_baseband_filter_bandwidth(device, DEFAULT_BASEBAND_FILTER_BANDWIDTH);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to set baseband filter bandwidth\n";
    }

    update_hackrf_gain_state();

    if (sweep_state) {
        delete sweep_state;
    }

    sweep_state = new hackrf_sweep_state();
    result = hackrf_sweep_easy_init(device, sweep_state);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to initialize sweep state\n";
    }

    hackrf_sweep_set_output(sweep_state, HACKRF_SWEEP_OUTPUT_MODE_BINARY, HACKRF_SWEEP_OUTPUT_TYPE_NOP, nullptr);

    uint16_t freq_range[] = {SWEEP_FREQ_MIN_MHZ, SWEEP_FREQ_MAX_MHZ};
    result = hackrf_sweep_set_range(sweep_state, freq_range, 1);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sweep range: " << result << "\n";
    }

    result = hackrf_sweep_setup_fft(sweep_state, FFTW_ESTIMATE, FFT_BIN_WIDTH);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to setup FFT\n";
    }
    return true;
}

void HackRF_Controller::start_sweep() {
    std::lock_guard<std::mutex> lock(device_mutex);
    if (!device || !sweep_state)
        return;

    hackrf_sweep_set_fft_rx_callback(sweep_state, hackrf_sweep_fft_callback);

    int result = hackrf_sweep_start(sweep_state, 0);  // 0 = infinite sweep
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to start sweep: " << result << "\n";
    }

    sweeping = true;
}

void HackRF_Controller::stop_sweep() {
    std::lock_guard<std::mutex> lock(device_mutex);

    if (!sweep_state)
        return;

    hackrf_sweep_stop(sweep_state);
    sweeping = false;
}

void HackRF_Controller::set_gain_state(const hackrf_gain_state state) {
    std::lock_guard<std::mutex> lock(device_mutex);
    gain_state = state;
    update_hackrf_gain_state();
}

void HackRF_Controller::set_amp_enable(bool enable) {
    std::lock_guard<std::mutex> lock(device_mutex);
    gain_state.amp_enable = enable;
    update_hackrf_gain_state();
}

void HackRF_Controller::set_vga_gain(int gain) {
    std::lock_guard<std::mutex> lock(device_mutex);
    if (gain % 2 != 0 || gain < 0 || gain > 62) {
        std::cerr << "Invalid VGA gain value: " << gain << "\n";
        return;
    }
    gain_state.vga_gain = gain;
    update_hackrf_gain_state();
}

void HackRF_Controller::set_lna_gain(int gain) {
    std::lock_guard<std::mutex> lock(device_mutex);
    if (gain % 8 != 0 || gain < 0 || gain > 40) {
        std::cerr << "Invalid LNA gain value: " << gain << "\n";
        return;
    }
    gain_state.lna_gain = gain;
    update_hackrf_gain_state();
}

int HackRF_Controller::get_total_gain() {
    std::lock_guard<std::mutex> lock(device_mutex);

    return gain_state.lna_gain + gain_state.vga_gain + (gain_state.amp_enable ? 14 : 0);
}

hackrf_gain_state HackRF_Controller::get_gain_state() {
    std::lock_guard<std::mutex> lock(device_mutex);

    return gain_state;
}

void HackRF_Controller::update_hackrf_gain_state() {
    if (!device) {
        std::cerr << "HackRF device is not connected.\n";
        return;
    }

    hackrf_set_amp_enable(device, gain_state.amp_enable);
    hackrf_set_vga_gain(device, gain_state.vga_gain);
    hackrf_set_lna_gain(device, gain_state.lna_gain);
}

void HackRF_Controller::set_fft_callback(fft_callback callback) {
    std::lock_guard<std::mutex> lock(device_mutex);
    fft_callback_ = callback;
}

void HackRF_Controller::handle_fft(uint64_t current_freq, int size, double fft_bin_width, float *pwr_ptr) {
    std::vector<double> x_data(size), y_data(size);

    for (int i = 0; i < size; ++i) {
        x_data[i] = current_freq / 1e6 + (i * fft_bin_width / 1e6);
        y_data[i] = pwr_ptr[i];
    }
    if (fft_callback_) {
        fft_callback_(x_data, y_data);
    }
}

