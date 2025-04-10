#ifndef DATASET_SPECTRUM_HPP
#define DATASET_SPECTRUM_HPP

#include <vector>
#include <cstdint>
#include <map>

#include <QMainWindow>
#include <QLineEdit>

#include <hackrf_sweeper.h>

class DatasetSpectrum {
   private:
    double fft_bin_size_hz;
    int freq_start_mhz;
    int freq_stop_mhz;
    std::map<uint64_t, float> spectrum;
    bool initialized = false;

   public:
    DatasetSpectrum();
    DatasetSpectrum(double fft_bin_size_hz, int freq_start_mhz, int freq_stop_mhz);

    int get_num_datapoints() const;
    void add_new_data(uint64_t start_freq, uint64_t end_freq, std::vector<float> pwr);
    const QVector<double> get_spectrum_array() const;
    const QVector<double> get_frequency_array() const;
    void clear();
    bool is_initialized() const;
};

#endif  // DATASET_SPECTRUM_HPP
