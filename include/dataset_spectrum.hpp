#ifndef DATASET_SPECTRUM_HPP
#define DATASET_SPECTRUM_HPP

#include <hackrf_sweeper.h>

#include <QLineEdit>
#include <QMainWindow>
#include <cstdint>
#include <map>
#include <vector>

class DatasetSpectrum {
   private:
    double fft_bin_size_hz;
    std::vector<uint16_t> freq_ranges;
    std::map<uint64_t, float> spectrum;
    bool initialized = false;

   public:
    DatasetSpectrum();
    DatasetSpectrum(double fft_bin_size_hz, std::vector<uint16_t> freq_ranges);

    int get_num_datapoints() const;
    int get_total_num_datapoints() const;
    void add_new_data(uint64_t start_freq, uint64_t end_freq, std::vector<float> pwr);
    std::map<uint64_t, float> get_spectrum() const;
    QVector<double> get_power_array() const;
    QVector<double> get_frequency_array() const;
    void clear();
    bool is_initialized() const;
};

#endif  // DATASET_SPECTRUM_HPP
