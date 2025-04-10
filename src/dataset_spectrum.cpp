#include "dataset_spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <QMainWindow>
#include <QLineEdit>

std::vector<int> int_linspace(double start, double stop, int num, bool endpoint = true) {
    // Return an empty vector if num is non-positive.
    if (num <= 0) {
        return std::vector<int>{};
    }

    // Special case for a single element.
    if (num == 1) {
        return std::vector<int>{static_cast<int>(start)};
    }

    std::vector<int> result(num);
    double step_size = endpoint ? (stop - start) / (num - 1) : (stop - start) / num;

    for (int i = 0; i < num; ++i) {
        result[i] = static_cast<int>(start + i * step_size);
    }

    return result;
}

DatasetSpectrum::DatasetSpectrum() : initialized(false) {}

DatasetSpectrum::DatasetSpectrum(double fft_bin_size_hz, int freq_start_mhz, int freq_stop_mhz)
    : fft_bin_size_hz(fft_bin_size_hz),
      freq_start_mhz(freq_start_mhz),
      freq_stop_mhz(freq_stop_mhz),
      initialized(true) {
}

int DatasetSpectrum::get_num_datapoints() const {
    return (freq_stop_mhz - freq_start_mhz) * 1e6l / fft_bin_size_hz;
}

void DatasetSpectrum::add_new_data(uint64_t start_freq, uint64_t end_freq, std::vector<float> pwr) {
    std::vector<int> freqs = int_linspace(start_freq, end_freq, pwr.size(), false);

    for (size_t i = 0; i < pwr.size(); ++i) {
        spectrum[freqs[i]] = pwr[i];
    }
}

const QVector<double> DatasetSpectrum::get_spectrum_array() const {
    QVector<double> spectrum_vec;

    spectrum_vec.reserve(spectrum.size());

    for (const auto& pair : spectrum) {
        spectrum_vec.push_back(pair.second);
    }

    return spectrum_vec;
}

const QVector<double> DatasetSpectrum::get_frequency_array() const {
    QVector<double> freq_vec;

    freq_vec.reserve(spectrum.size());

    std::cout << spectrum.size() << ' ' << get_num_datapoints() << '\n';
    for (const auto& pair : spectrum) {
        freq_vec.push_back(pair.first / 1e6);
        // std::cout << pair.first << '\n';
    }
    std::cout << freq_vec[0] << ' ' << freq_vec[freq_vec.size() - 1] << '\n';

    return freq_vec;
}

void DatasetSpectrum::clear() {
    spectrum.clear();
}

bool DatasetSpectrum::is_initialized() const {
    return initialized;
}
