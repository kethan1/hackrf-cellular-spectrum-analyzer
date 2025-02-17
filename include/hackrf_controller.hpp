#ifndef HackRF_Controller_H
#define HackRF_Controller_H

#include <libhackrf/hackrf.h>

#include <functional>
#include <mutex>
#include <vector>
extern "C" {
#include <hackrf_sweeper.h>
}

const int FFT_BIN_WIDTH = 10'000;
const uint16_t SWEEP_FREQ_MIN_MHZ = 2'400;
const uint16_t SWEEP_FREQ_MAX_MHZ = 2'500;

/**
 * Callback type to deliver FFT data to an observer.
 * x_data and y_data hold the FFT data.
 */
using fft_callback = std::function<void(const std::vector<double> &x_data, const std::vector<double> &y_data)>;

struct hackrf_gain_state {
    bool amp_enable;
    int vga_gain;
    int lna_gain;
};

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
    void handle_fft(uint64_t current_freq, int size, double fft_bin_width, float *pwr_ptr);

    void set_fft_callback(fft_callback callback);
};

#endif  // HackRF_Controller_H
