#ifndef HACKRF_CONTROLLER_HPP
#define HACKRF_CONTROLLER_HPP

#include <libhackrf/hackrf.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

extern "C" {
#include <hackrf_sweeper.h>
}

namespace sweep_config {
constexpr int FFT_BIN_WIDTH_HZ = 50'000;
constexpr uint16_t SWEEP_FREQ_MIN_MHZ = 2'200;
constexpr uint16_t SWEEP_FREQ_MAX_MHZ = 2'600;
}  // namespace sweep_config

namespace gain_limits {
constexpr int VGA_MIN = 0;
constexpr int VGA_MAX = 62;
constexpr int VGA_STEP = 2;
constexpr int LNA_MIN = 0;
constexpr int LNA_MAX = 40;
constexpr int LNA_STEP = 8;
constexpr int AMP_GAIN_DB = 14;
}  // namespace gain_limits

class HackRFGainState {
   public:
    bool amp_enable = false;
    int vga_gain = 32;  // Valid: 0-62, must be even
    int lna_gain = 0;   // Valid: 0-40, must be multiple of 8

    [[nodiscard]] int total_gain() const noexcept {
        return lna_gain + vga_gain + (amp_enable ? gain_limits::AMP_GAIN_DB : 0);
    }

    [[nodiscard]] bool is_valid() const noexcept {
        const bool vga_valid = (vga_gain >= gain_limits::VGA_MIN) &&
                               (vga_gain <= gain_limits::VGA_MAX) &&
                               (vga_gain % gain_limits::VGA_STEP == 0);
        const bool lna_valid = (lna_gain >= gain_limits::LNA_MIN) &&
                               (lna_gain <= gain_limits::LNA_MAX) &&
                               (lna_gain % gain_limits::LNA_STEP == 0);
        return vga_valid && lna_valid;
    }
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

    void set_gain_state(const HackRFGainState& state);
    void set_amp_enable(bool enable);
    bool set_vga_gain(int gain);
    bool set_lna_gain(int gain);
    [[nodiscard]] int get_total_gain() const;
    [[nodiscard]] HackRFGainState get_gain_state() const;

    void set_fft_callback(FFTCallback callback);
    [[nodiscard]] FFTCallback get_fft_callback() const;

   private:
    void update_device_gain();  // Must be called with mutex held
    void cleanup_device();      // Must be called with mutex held

    hackrf_device* device_ = nullptr;
    std::unique_ptr<hackrf_sweep_state_t> sweep_state_;
    HackRFGainState gain_state_;
    bool connected_ = false;
    bool sweeping_ = false;
    mutable std::mutex mutex_;
    FFTCallback fft_callback_;
};

#endif  // HACKRF_CONTROLLER_HPP
