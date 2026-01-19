#ifndef HACKRF_CONTROLLER_HPP
#define HACKRF_CONTROLLER_HPP

#include <libhackrf/hackrf.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "hackrf_gain_state.hpp"

extern "C" {
#include <hackrf_sweeper.h>
}

constexpr int FFT_BIN_WIDTH_HZ = 50'000;

namespace hackrf_hardware {
constexpr int VGA_MIN = 0;
constexpr int VGA_MAX = 62;
constexpr int VGA_STEP = 2;
constexpr int LNA_MIN = 0;
constexpr int LNA_MAX = 40;
constexpr int LNA_STEP = 8;
constexpr int AMP_GAIN_DB = 14;
}  // namespace hackrf_hardware

struct ScanRange {
    uint16_t start_mhz;
    uint16_t end_mhz;
};

struct FrequencyBand {
    uint64_t start_hz = 0;
    uint64_t end_hz = 0;
    std::vector<float> power_db;
};

struct FFTSweepData {
    double bin_width_hz = 0.0;
    int fft_size = 0;
    std::vector<uint16_t> freq_ranges_mhz;
    FrequencyBand band_lower;
    FrequencyBand band_upper;
};

using FFTCallback = std::function<void(const FFTSweepData& data)>;

class HackRFController {
   public:
    HackRFController();
    ~HackRFController();

    // Non-copyable, non-movable due to mutex and device handle
    HackRFController(const HackRFController&) = delete;
    HackRFController& operator=(const HackRFController&) = delete;
    HackRFController(HackRFController&&) = delete;
    HackRFController& operator=(HackRFController&&) = delete;

    [[nodiscard]] bool is_connected() const;
    bool connect_device();

    void start_sweep();
    void stop_sweep();
    void restart_sweep();

    void set_gain_state(const HackRFGainState& state);
    [[nodiscard]] HackRFGainState get_gain_state() const;
    void set_amp_enable(bool enable) noexcept;
    void set_vga_gain(int gain);
    void set_lna_gain(int gain);

    void set_fft_callback(FFTCallback callback);
    [[nodiscard]] FFTCallback get_fft_callback() const;

    bool set_scan_ranges(const std::vector<ScanRange>& ranges);
    [[nodiscard]] std::vector<ScanRange> get_scan_ranges() const;

   private:
    void update_device_gain();  // Must be called with mutex held
    bool update_device_scan_ranges();
    void cleanup_device();  // Must be called with mutex held

    hackrf_device* device_ = nullptr;
    std::unique_ptr<hackrf_sweep_state_t> sweep_state_;
    HackRFGainState gain_state_;
    std::vector<ScanRange> scan_ranges_;
    bool sweeping_ = false;
    mutable std::mutex mutex_;
    FFTCallback fft_callback_;
};

#endif  // HACKRF_CONTROLLER_HPP
