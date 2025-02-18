#include "main_window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QSlider>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <algorithm>
#include <iostream>
#include <ranges>

#include "dataset_spectrum.hpp"
#include "hackrf_controller.hpp"

MainWindow::MainWindow(HackRF_Controller *ctrl, QWidget *parent) : QMainWindow(parent), controller(ctrl) {
    QWidget *central_widget = new QWidget(this);
    QVBoxLayout *main_layout = new QVBoxLayout(central_widget);

    QFormLayout *control_layout = new QFormLayout();

    QCheckBox *amp_check_box = new QCheckBox("AMP");
    amp_check_box->setChecked(controller->get_gain_state().amp_enable);
    connect(amp_check_box, &QCheckBox::stateChanged, [this](int state) {
        controller->set_amp_enable(state == Qt::Checked);
        update_total_gain();
    });
    control_layout->addRow(amp_check_box);

    QLabel *lna_label = new QLabel("LNA Gain");
    QSlider *lna_slider = new QSlider(Qt::Horizontal);
    lna_slider->setRange(0, 5);  // 40 / 8 = 5 steps.
    lna_slider->setValue(controller->get_gain_state().lna_gain / 8);
    lna_slider->setTickInterval(1);
    lna_slider->setSingleStep(1);
    connect(lna_slider, &QSlider::valueChanged, [this](int value) {
        controller->set_lna_gain(value * 8);
        update_total_gain();
    });
    control_layout->addRow(lna_label, lna_slider);

    QLabel *vga_label = new QLabel("VGA Gain");
    QSlider *vga_slider = new QSlider(Qt::Horizontal);
    vga_slider->setRange(0, 31);  // 62 / 2 = 31 steps.
    vga_slider->setValue(controller->get_gain_state().vga_gain / 2);
    vga_slider->setTickInterval(1);
    vga_slider->setSingleStep(1);
    connect(vga_slider, &QSlider::valueChanged, [this](int value) {
        controller->set_vga_gain(value * 2);
        update_total_gain();
    });
    control_layout->addRow(vga_label, vga_slider);

    QLabel *total_gain_label = new QLabel("Total Gain:");
    total_gain_field = new QLineEdit();
    total_gain_field->setReadOnly(true);
    control_layout->addRow(total_gain_label, total_gain_field);
    update_total_gain();

    main_layout->addLayout(control_layout);

    custom_plot = new QwtPlot();
    custom_plot->setTitle("Frequency Sweep");
    custom_plot->setAxisTitle(QwtPlot::xBottom, "Frequency (MHz)");
    custom_plot->setAxisTitle(QwtPlot::yLeft, "Power (dB)");
    custom_plot->setAxisScale(QwtPlot::xBottom, SWEEP_FREQ_MIN_MHZ * 1e6, SWEEP_FREQ_MAX_MHZ * 1e6);
    custom_plot->setAxisScale(QwtPlot::yLeft, -110, 20);

    curve = new QwtPlotCurve();
    curve->setTitle("Sweep Data");
    curve->attach(custom_plot);

    main_layout->addWidget(custom_plot);

    color_plot = new QwtPlot();
    color_plot->setTitle("Spectrogram");
    color_plot->setAxisTitle(QwtPlot::xBottom, "Frequency (MHz)");
    color_plot->setAxisTitle(QwtPlot::yLeft, "Time");
    color_plot->setAxisScale(QwtPlot::xBottom, SWEEP_FREQ_MIN_MHZ * 1e6, SWEEP_FREQ_MAX_MHZ * 1e6);
    color_plot->setAxisScale(QwtPlot::yLeft, 0, color_map_samples);

    color_map = new QwtPlotSpectrogram();
    raster_data = new QwtMatrixRasterData();
    raster_data->setInterval(Qt::XAxis, QwtInterval(0, color_map_width));
    raster_data->setInterval(Qt::YAxis, QwtInterval(0, color_map_samples));
    raster_data->setInterval(Qt::ZAxis, QwtInterval(-110, 25));

    QwtLinearColorMap *linear_color_map = new QwtLinearColorMap();
    linear_color_map->addColorStop(0.0, QColor(0, 0, 50));
    linear_color_map->addColorStop(0.15, QColor(20, 0, 120));
    linear_color_map->addColorStop(0.33, QColor(200, 30, 140));
    linear_color_map->addColorStop(0.6, QColor(255, 100, 0));
    linear_color_map->addColorStop(0.85, QColor(255, 255, 40));
    linear_color_map->addColorStop(1.0, QColor(255, 255, 255));
    color_map->setColorMap(linear_color_map);

    QVector<double> matrix_data(color_map_width * color_map_samples, -110);
    raster_data->setValueMatrix(matrix_data, color_map_width);
    color_map->setData(raster_data);

    color_map->attach(color_plot);

    main_layout->addWidget(color_plot);

    setCentralWidget(central_widget);

    controller->set_fft_callback([this](const hackrf_sweep_state_t *state, uint64_t current_freq) {
        QMetaObject::invokeMethod(this, [=]() { update_plot(state, current_freq); }, Qt::QueuedConnection);
    });
}

void MainWindow::update_plot(const hackrf_sweep_state_t *state, uint64_t current_freq) {
    int size = state->fft.size;
    int fft_bin_width = state->fft.bin_width;
    float *pwr_ptr = state->fft.pwr;

    if (!dataset_spectrum.is_initialized()) {
        dataset_spectrum = DatasetSpectrum(fft_bin_width, SWEEP_FREQ_MIN_MHZ, SWEEP_FREQ_MAX_MHZ, -120);
    }

    std::vector<double> x_data(size), y_data(size);

    for (int i = 0; i < size; ++i) {
        x_data[i] = current_freq + i * fft_bin_width;
        y_data[i] = pwr_ptr[i];
    }

    if (dataset_spectrum.add_new_data(x_data, y_data)) {
        const auto freq_arr = dataset_spectrum.get_frequency_array();
        const auto &pwr_arr = dataset_spectrum.get_spectrum_array();

        curve->setSamples(
            QVector(freq_arr.begin(), freq_arr.end()),
            QVector(pwr_arr.begin(), pwr_arr.end())
        );
        custom_plot->replot();
    }

    QVector<double> matrix_data = raster_data->valueMatrix();

    for (int row = color_map_samples - 1; row > 0; --row) {
        for (size_t col = 0; col < color_map_width; ++col) {
            matrix_data[row * color_map_width + col] = matrix_data[(row - 1) * color_map_width + col];
        }
    }

    int col = 0;
    for (double value : dataset_spectrum.get_spectrum_array()) {
        matrix_data[col] = value;
        ++col;
    }
    raster_data->setValueMatrix(matrix_data, color_map_width);
    color_plot->replot();
}

void MainWindow::update_total_gain() {
    total_gain_field->setText(QString::number(controller->get_total_gain()) + " dB");
}
