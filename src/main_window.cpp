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
#include "waterfall_raster_data.hpp"

class ThermalColorMap : public QwtLinearColorMap {
   public:
    ThermalColorMap() {
        addColorStop(0.0, QColor(0, 0, 50));
        addColorStop(0.15, QColor(20, 0, 120));
        addColorStop(0.33, QColor(200, 30, 140));
        addColorStop(0.6, QColor(255, 100, 0));
        addColorStop(0.85, QColor(255, 255, 40));
        addColorStop(1, QColor(255, 255, 255));
    }
};

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

    color_map = new QwtPlotSpectrogram();
    color_map->setColorMap(new ThermalColorMap());
    color_map->attach(color_plot);

    main_layout->addWidget(color_plot);

    setCentralWidget(central_widget);

    controller->set_fft_callback([this](const hackrf_sweep_state_t *state, uint64_t current_freq) {
        QMetaObject::invokeMethod(this, [this, state, current_freq]() { update_plot(state, current_freq); }, Qt::QueuedConnection);
    });
}

void MainWindow::update_plot(const hackrf_sweep_state_t *state, uint64_t current_freq) {
    int size = state->fft.size;
    int fft_bin_width = state->fft.bin_width;
    float *pwr_ptr = state->fft.pwr;

    if (!dataset_spectrum.is_initialized()) {
        dataset_spectrum = DatasetSpectrum(fft_bin_width, state->frequencies[0], state->frequencies[1], -90);

        raster_data = new WaterfallRasterData(COLOR_MAP_SAMPLES, dataset_spectrum.get_num_datapoints(), -90);

        raster_data->setInterval(Qt::ZAxis, QwtInterval(-90, -25));
        color_plot->setAxisScale(QwtPlot::xBottom, 0, dataset_spectrum.get_num_datapoints());

        color_map->setData(raster_data);
    }

    if (dataset_spectrum.add_new_data(current_freq, pwr_ptr, size)) {
        const std::vector<double> freq_vec = dataset_spectrum.get_frequency_array();
        const auto &pwr_vec = dataset_spectrum.get_spectrum_array();

        curve->setSamples(
            QVector(freq_vec.begin(), freq_vec.end()),
            QVector(pwr_vec.begin(), pwr_vec.end()));
        custom_plot->replot();

        QVector<double> *freq_qvec = new QVector<double>(pwr_vec.begin(), pwr_vec.end());

        raster_data->addRow(freq_qvec);
        color_plot->replot();
    }
}

void MainWindow::update_total_gain() {
    total_gain_field->setText(QString::number(controller->get_total_gain()) + " dB");
}
