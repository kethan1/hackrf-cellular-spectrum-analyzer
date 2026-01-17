#include "hackrf_controller.hpp"

#include <iostream>

namespace {

FrequencyBand extract_band(const hackrf_sweep_state_t* state,
                           uint64_t start_freq,
                           int power_offset,
                           int num_bins) {
    FrequencyBand band;
    band.start_hz = start_freq;
    band.end_hz = start_freq + state->sample_rate_hz / 4;
    band.power_db.reserve(num_bins);
    band.power_db.insert(
        band.power_db.begin(),
        &state->fft.pwr[1 + power_offset],
        &state->fft.pwr[1 + power_offset + num_bins]);
    return band;
}

std::vector<uint16_t> build_freq_ranges(const hackrf_sweep_state_t* state) {
    std::vector<uint16_t> ranges;
    ranges.reserve(static_cast<size_t>(state->num_ranges) * 2);

    for (int i = 0; i < state->num_ranges; ++i) {
        ranges.push_back(state->frequencies[i * 2]);
        ranges.push_back(state->frequencies[i * 2 + 1]);
    }
    return ranges;
}

}  // namespace

extern "C" {
    int hackrf_sweep_fft_callback(void* sweep_state, uint64_t current_freq, hackrf_transfer* /*transfer*/) {
        hackrf_sweep_state_t* state = static_cast<hackrf_sweep_state_t*>(sweep_state);

        if (!state || !state->user_ctx) {
            return 0;
        }

        HackRFController* controller = static_cast<HackRFController*>(state->user_ctx);
        const FFTCallback callback = controller->get_fft_callback();

        if (!callback) {
            return 0;
        }

        const int quarter_fft = state->fft.size / 4;

        FFTSweepData data;
        data.bin_width_hz = state->fft.bin_width;
        data.fft_size = state->fft.size;
        data.freq_ranges_mhz = build_freq_ranges(state);

        // Lower band: offset at 5/8 of FFT size
        data.band_lower = extract_band(
            state,
            current_freq,
            state->fft.size * 5 / 8,
            quarter_fft);

        // Upper band: starts at sample_rate/2, offset at 1/8 of FFT size
        data.band_upper = extract_band(
            state,
            current_freq + state->sample_rate_hz / 2,
            state->fft.size / 8,
            quarter_fft);

        callback(data);
        return 0;
    }
}

HackRFController::HackRFController() = default;

HackRFController::~HackRFController() {
    stop_sweep();

    std::lock_guard<std::mutex> lock(mutex_);
    cleanup_device();
}

bool HackRFController::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

bool HackRFController::connect_device() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connected_ && device_) {
        cleanup_device();
    }

    const int result = hackrf_open(&device_);
    if (result != HACKRF_SUCCESS) {
        std::cerr << "HackRF device not connected: " << result << "\n";
        device_ = nullptr;
        connected_ = false;
        return false;
    }

    connected_ = true;

    if (hackrf_set_sample_rate_manual(device_, DEFAULT_SAMPLE_RATE_HZ, 1) != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sample rate\n";
    }

    if (hackrf_set_baseband_filter_bandwidth(device_, DEFAULT_BASEBAND_FILTER_BANDWIDTH) != HACKRF_SUCCESS) {
        std::cerr << "Failed to set baseband filter bandwidth\n";
    }

    update_device_gain();

    sweep_state_ = std::make_unique<hackrf_sweep_state_t>();
    if (hackrf_sweep_easy_init(device_, sweep_state_.get()) != HACKRF_SUCCESS) {
        std::cerr << "Failed to initialize sweep state\n";
    }

    hackrf_sweep_set_output(sweep_state_.get(), HACKRF_SWEEP_OUTPUT_MODE_TEXT, HACKRF_SWEEP_OUTPUT_TYPE_NOP, nullptr);
    sweep_state_->user_ctx = this;

    if (hackrf_sweep_set_fft_rx_callback(sweep_state_.get(), hackrf_sweep_fft_callback) != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sweep FFT RX callback\n";
    }

    uint16_t freq_ranges[] = {sweep_config::SWEEP_FREQ_MIN_MHZ, sweep_config::SWEEP_FREQ_MAX_MHZ};
    if (hackrf_sweep_set_range(sweep_state_.get(), freq_ranges, 1) != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sweep range\n";
    }

    if (hackrf_sweep_setup_fft(sweep_state_.get(), FFTW_ESTIMATE, sweep_config::FFT_BIN_WIDTH_HZ) != HACKRF_SUCCESS) {
        std::cerr << "Failed to setup FFT\n";
    }

    return true;
}

void HackRFController::start_sweep() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_ || !sweep_state_) {
        return;
    }

    const int result = hackrf_sweep_start(sweep_state_.get(), 0);  // 0 = infinite sweep
    if (result != HACKRF_SUCCESS) {
        std::cerr << "Failed to start sweep: " << result << "\n";
        return;
    }

    sweeping_ = true;
}

void HackRFController::stop_sweep() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!sweep_state_ || !device_) {
        return;
    }

    hackrf_sweep_stop(sweep_state_.get());
    sweeping_ = false;
}

void HackRFController::set_gain_state(const HackRFGainState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    gain_state_ = state;
    update_device_gain();
}

void HackRFController::set_amp_enable(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    gain_state_.amp_enable = enable;
    update_device_gain();
}

bool HackRFController::set_vga_gain(int gain) {
    std::lock_guard<std::mutex> lock(mutex_);

    const bool valid = (gain % gain_limits::VGA_STEP == 0) &&
                       (gain >= gain_limits::VGA_MIN) &&
                       (gain <= gain_limits::VGA_MAX);
    if (!valid) {
        std::cerr << "Invalid VGA gain value: " << gain
                  << " (must be even, 0-62)\n";
        return false;
    }

    gain_state_.vga_gain = gain;
    update_device_gain();
    return true;
}

bool HackRFController::set_lna_gain(int gain) {
    std::lock_guard<std::mutex> lock(mutex_);

    const bool valid = (gain % gain_limits::LNA_STEP == 0) &&
                       (gain >= gain_limits::LNA_MIN) &&
                       (gain <= gain_limits::LNA_MAX);
    if (!valid) {
        std::cerr << "Invalid LNA gain value: " << gain
                  << " (must be multiple of 8, 0-40)\n";
        return false;
    }

    gain_state_.lna_gain = gain;
    update_device_gain();
    return true;
}

int HackRFController::get_total_gain() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gain_state_.total_gain();
}

HackRFGainState HackRFController::get_gain_state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gain_state_;
}

void HackRFController::update_device_gain() {
    if (!device_) {
        std::cerr << "HackRF device is not connected.\n";
        return;
    }

    hackrf_set_amp_enable(device_, gain_state_.amp_enable ? 1 : 0);
    hackrf_set_vga_gain(device_, static_cast<uint32_t>(gain_state_.vga_gain));
    hackrf_set_lna_gain(device_, static_cast<uint32_t>(gain_state_.lna_gain));
}

void HackRFController::cleanup_device() {
    if (device_) {
        hackrf_close(device_);
        device_ = nullptr;
    }
    connected_ = false;
    sweeping_ = false;
}

void HackRFController::set_fft_callback(FFTCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    fft_callback_ = std::move(callback);
}

FFTCallback HackRFController::get_fft_callback() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fft_callback_;
}
