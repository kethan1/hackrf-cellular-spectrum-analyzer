#include "dataset_spectrum.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
#include <QMainWindow>
#include <QLineEdit>

template <class T>
std::vector<uint64_t> linspace(T start, T stop, int num, bool endpoint = true) {
    // Return an empty vector if num is non-positive.
    if (num <= 0) {
        return std::vector<T>{};
    }

    // Special case for a single element.
    if (num == 1) {
        return std::vector<T>{static_cast<T>(start)};
    }

    std::vector<T> result(num);
    double step_size = endpoint ? (stop - start) / (num - 1) : (stop - start) / num;

    for (int i = 0; i < num; ++i) {
        result[i] = static_cast<T>(start + i * step_size);
    }

    return result;
}

DatasetSpectrum::DatasetSpectrum() : initialized(false) {}

DatasetSpectrum::DatasetSpectrum(double fft_bin_size_hz, std::vector<uint16_t> freq_ranges)
    : fft_bin_size_hz(fft_bin_size_hz),
      freq_ranges(freq_ranges),
      initialized(true) {
}

int DatasetSpectrum::get_num_datapoints() const {
    int datapoints = 0;
    for (size_t i = 0; i < freq_ranges.size(); i += 2) {
        datapoints += (freq_ranges[i + 1] - freq_ranges[i]) / (fft_bin_size_hz / 1e6);
    }
    return datapoints;
}

int DatasetSpectrum::get_total_num_datapoints() const {
    return (freq_ranges[freq_ranges.size() - 1] - freq_ranges[0]) / (fft_bin_size_hz / 1e6);
}

void DatasetSpectrum::add_new_data(uint64_t start_freq, uint64_t end_freq, std::vector<float> pwr) {
    std::vector<uint64_t> freqs = linspace<uint64_t>(start_freq, end_freq, pwr.size(), false);

    for (size_t i = 0; i < pwr.size(); ++i) {
        spectrum[freqs[i]] = pwr[i];
    }
}

std::map<uint64_t, float> DatasetSpectrum::get_spectrum() const {
    return spectrum;
}

QVector<double> DatasetSpectrum::get_power_array() const {
    QVector<double> spectrum_vec;

    spectrum_vec.reserve(spectrum.size());

    for (const auto& pair : spectrum) {
        spectrum_vec.push_back(pair.second);
    }

    return spectrum_vec;
}

QVector<double> DatasetSpectrum::get_frequency_array() const {
    QVector<double> freq_vec;

    freq_vec.reserve(spectrum.size());

    for (const auto& pair : spectrum) {
        freq_vec.push_back(pair.first / 1e6);
    }

    return freq_vec;
}

void DatasetSpectrum::clear() {
    spectrum.clear();
}

bool DatasetSpectrum::is_initialized() const {
    return initialized;
}
