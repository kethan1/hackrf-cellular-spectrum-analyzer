#ifndef DATASET_SPECTRUM_HPP
#define DATASET_SPECTRUM_HPP

#include <vector>

class DatasetSpectrum {
   private:
    double fft_bin_size_hz;
    long freq_start_hz;
    int freq_start_mhz;
    int freq_stop_mhz;
    std::vector<double> spectrum;
    double spectrum_init_power;
    bool initialized = false;

   public:
    DatasetSpectrum();
    DatasetSpectrum(double fft_bin_size_hz, int freq_start_mhz, int freq_stop_mhz, double spectrum_init_power);

    int get_num_datapoints();
    bool add_new_data(const std::vector<double> &freq_start, const std::vector<double> &sig_pow_dbm);
    bool add_new_data(double freq_start, const std::vector<double> &sig_pow_dbm);
    const std::vector<double> &get_spectrum_array() const;
    const std::vector<double> get_frequency_array() const;
    void clear();
    bool is_initialized() const;
};

#endif  // DATASET_SPECTRUM_HPP
