#ifndef HackRF_Controller_HPP
#define HackRF_Controller_HPP

#include <functional>
#include <mutex>
#include <vector>
#include <cstdint>

#include <libhackrf/hackrf.h>
extern "C" {
#include <hackrf_sweeper.h>
}

const int FFT_BIN_WIDTH = 50'000;
const uint16_t SWEEP_FREQ_MIN_MHZ = 2'200;
const uint16_t SWEEP_FREQ_MAX_MHZ = 2'600;

struct hackrf_gain_state {
    bool amp_enable;
    int vga_gain;
    int lna_gain;
};

struct db_data {
    double bin_width;
    int fft_size;
    std::vector<uint16_t> freq_ranges;
    uint64_t start_1;
    uint64_t end_1;
    std::vector<float> pwr_1;
    uint64_t start_2;
    uint64_t end_2;
    std::vector<float> pwr_2;
};

/**
 * Callback type to deliver FFT data to an observer.
 */
using fft_callback = std::function<void(db_data data)>;

class HackRF_Controller {
   private:
    hackrf_device *device = nullptr;
    hackrf_sweep_state_t *sweep_state = nullptr;
    hackrf_gain_state gain_state;
    bool connected;
    bool sweeping;
    std::mutex device_mutex;
    fft_callback fft_callback_;

    void update_hackrf_gain_state();

    public:
    HackRF_Controller();
    ~HackRF_Controller();

    bool connect_device();
    void start_sweep();
    void stop_sweep();

    void set_gain_state(const hackrf_gain_state state);
    void set_amp_enable(bool enable);
    void set_vga_gain(int gain);  // must be even, 0 <= gain <= 62
    void set_lna_gain(int gain);  // must be multiple of 8, 0 <= gain <= 40
    int get_total_gain();
    hackrf_gain_state get_gain_state();

    void set_fft_callback(fft_callback callback);
    fft_callback get_fft_callback();
};

#endif  // HackRF_Controller_HPP
