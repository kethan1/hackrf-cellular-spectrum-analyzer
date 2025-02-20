#include "dataset_spectrum.hpp"

#include <cmath>
#include <vector>
#include <iostream>

DatasetSpectrum::DatasetSpectrum() : initialized(false) {}

DatasetSpectrum::DatasetSpectrum(double fft_bin_size_hz, int freq_start_mhz, int freq_stop_mhz, double spectrum_init_power)
    : fft_bin_size_hz(fft_bin_size_hz),
      freq_start_mhz(freq_start_mhz),
      freq_stop_mhz(freq_stop_mhz),
      spectrum_init_power(spectrum_init_power),
      initialized(true),
      freq_start_hz(freq_start_mhz * 1e6l) {
    int datapoints = std::ceil(freq_stop_mhz - freq_start_mhz) * 1e6 / fft_bin_size_hz;
    spectrum.resize(datapoints, spectrum_init_power);
}

int DatasetSpectrum::get_num_datapoints() {
    return std::ceil(freq_stop_mhz - freq_start_mhz) * 1e6 / fft_bin_size_hz;
}

bool DatasetSpectrum::add_new_data(const std::vector<double> &freq_start, const std::vector<double> &sig_pow_dbm) {
    for (size_t bins_index = 0; bins_index < freq_start.size(); ++bins_index) {
        double freq_start_val = freq_start[bins_index];
        int spectr_index = (freq_start_val - freq_start_hz) / fft_bin_size_hz;
        if (spectr_index < 0 || spectr_index >= spectrum.size())
            continue;
        spectrum[spectr_index] = sig_pow_dbm[bins_index];
    }

    return freq_start[0] == freq_start_hz;
}

bool DatasetSpectrum::add_new_data(double freq_start, const std::vector<double> &sig_pow_dbm) {
    int spectr_index = (freq_start - freq_start_hz) / fft_bin_size_hz;
    for (size_t bins_index = 0; bins_index < sig_pow_dbm.size(); ++bins_index) {
        spectrum[spectr_index + bins_index] = sig_pow_dbm[bins_index];
    }

    return freq_start == freq_start_hz;
}

const std::vector<double> &DatasetSpectrum::get_spectrum_array() const {
    return spectrum;
}

const std::vector<double> DatasetSpectrum::get_frequency_array() const {
    std::vector<double> freq_arr;

    for (size_t i = 0; i < spectrum.size(); ++i) {
        freq_arr.push_back(freq_start_hz + fft_bin_size_hz * i);
    }

    return freq_arr;
}

void DatasetSpectrum::clear() {
    std::fill(spectrum.begin(), spectrum.end(), spectrum_init_power);
}

bool DatasetSpectrum::is_initialized() const {
    return initialized;
}
