#include "hackrf_controller.hpp"

#include <libhackrf/hackrf.h>

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>
#include <thread>

#include "hackrf_gain_state.hpp"

extern "C" {
#include <hackrf_sweeper.h>
}

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
    return device_ != nullptr;
}

bool HackRFController::connect_device() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (device_) {
        cleanup_device();
    }

    int ret = hackrf_open(&device_);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "HackRF device not connected: " << ret << '\n';
        device_ = nullptr;
        return false;
    }

    ret = hackrf_set_sample_rate_manual(device_, DEFAULT_SAMPLE_RATE_HZ, 1);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sample rate: " << ret << '\n';
    }

    ret = hackrf_set_baseband_filter_bandwidth(device_, DEFAULT_BASEBAND_FILTER_BANDWIDTH);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to set baseband filter bandwidth: " << ret << '\n';
    }

    update_device_gain();

    sweep_state_ = std::make_unique<hackrf_sweep_state_t>();

    ret = hackrf_sweep_easy_init(device_, sweep_state_.get());
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to initialize sweep state: " << ret << '\n';
    }

    sweep_state_->user_ctx = this;

    ret = hackrf_sweep_set_output(sweep_state_.get(), HACKRF_SWEEP_OUTPUT_MODE_TEXT, HACKRF_SWEEP_OUTPUT_TYPE_NOP, nullptr);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sweep output: " << ret << '\n';
    }

    ret = hackrf_sweep_set_fft_rx_callback(sweep_state_.get(), hackrf_sweep_fft_callback);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to set sweep FFT RX callback" << ret << '\n';
    }

    update_device_scan_ranges();

    ret = hackrf_sweep_setup_fft(sweep_state_.get(), FFTW_ESTIMATE, FFT_BIN_WIDTH_HZ);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to setup FFT: " << ret << '\n';
    }

    return true;
}

void HackRFController::start_sweep() {
    std::unique_lock<std::mutex> lock(mutex_);

    if (!device_ || !sweep_state_) {
        return;
    }

    int ret = hackrf_sweep_start(sweep_state_.get(), 0);  // 0 = infinite sweep
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to start sweep: " << ret << "\n";
        return;
    }

    sweeping_ = true;
}

void HackRFController::stop_sweep() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!sweep_state_ || !device_) {
        return;
    }

    int ret = hackrf_sweep_stop(sweep_state_.get());

    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to stop sweep: " << ret << "\n";
        return;
    }

    sweeping_ = false;
}

void HackRFController::set_gain_state(const HackRFGainState& state) {
    std::lock_guard<std::mutex> lock(mutex_);
    gain_state_ = state;
    update_device_gain();
}

void HackRFController::set_amp_enable(bool enable) noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    gain_state_.set_amp_enable(enable);
    lock.unlock();

    update_device_gain();
}

HackRFGainState HackRFController::get_gain_state() const {
    std::unique_lock<std::mutex> lock(mutex_);
    lock.unlock();

    return gain_state_;
}

void HackRFController::set_vga_gain(int gain) {
    std::unique_lock<std::mutex> lock(mutex_);
    gain_state_.set_vga_gain(gain);
    lock.unlock();

    update_device_gain();
}

void HackRFController::set_lna_gain(int gain) {
    std::unique_lock<std::mutex> lock(mutex_);
    gain_state_.set_lna_gain(gain);
    lock.unlock();

    update_device_gain();
}

void HackRFController::update_device_gain() {
    if (!device_) {
        std::cerr << "HackRF device is not connected.\n";
        return;
    }

    hackrf_set_amp_enable(device_, gain_state_.get_amp_enable() ? 1 : 0);
    hackrf_set_vga_gain(device_, static_cast<uint32_t>(gain_state_.get_vga_gain()));
    hackrf_set_lna_gain(device_, static_cast<uint32_t>(gain_state_.get_lna_gain()));
}

void HackRFController::cleanup_device() {
    if (device_) {
        hackrf_sweep_close(sweep_state_.get());
        hackrf_close(device_);
        device_ = nullptr;
    }
    sweep_state_.reset();
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

bool HackRFController::set_scan_ranges(const std::vector<ScanRange>& ranges) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (ranges.empty()) {
        std::cerr << "At least one scan range is required\n";
        return false;
    }

    for (const ScanRange& range : ranges) {
        if (range.start_mhz >= range.end_mhz) {
            std::cerr << "Invalid range: start must be less than end\n";
            return false;
        }
        if (range.start_mhz < FREQ_MIN_MHZ ||
            range.end_mhz > FREQ_MAX_MHZ) {
            std::cerr << "Frequency out of HackRF range (1-6000 MHz)\n";
            return false;
        }
    }

    scan_ranges_ = ranges;

    return update_device_scan_ranges();
}

bool HackRFController::update_device_scan_ranges() {
    std::vector<uint16_t> freq_ranges;
    freq_ranges.reserve(scan_ranges_.size() * 2);

    for (const ScanRange& range : scan_ranges_) {
        freq_ranges.push_back(range.start_mhz);
        freq_ranges.push_back(range.end_mhz);
    }

    if (sweep_state_ && device_) {
        int ret = hackrf_sweep_set_range(sweep_state_.get(), freq_ranges.data(), static_cast<int>(scan_ranges_.size()));
        if (ret != HACKRF_SUCCESS) {
            std::cerr << "Failed to set sweep range: " << ret << '\n';
            return false;
        }
    }

    return true;
}

std::vector<ScanRange> HackRFController::get_scan_ranges() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return scan_ranges_;
}

void HackRFController::restart_sweep() {
    stop_sweep();

    std::lock_guard<std::mutex> lock(mutex_);

    if (!device_ || !sweep_state_) {
        return;
    }

    update_device_scan_ranges();

    int ret = hackrf_sweep_start(sweep_state_.get(), 0);
    if (ret != HACKRF_SUCCESS) {
        std::cerr << "Failed to restart sweep: " << ret << "\n";
        return;
    }

    sweeping_ = true;
}
