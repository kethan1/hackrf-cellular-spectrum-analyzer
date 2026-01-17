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

#include "thermal_color_map.hpp"

MainWindow::MainWindow(HackRFController* ctrl, QWidget* parent)
    : QMainWindow(parent), controller_(ctrl) {
    auto* central_widget = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central_widget);

    auto* control_layout = new QFormLayout();

    auto* amp_check_box = new QCheckBox("AMP");
    amp_check_box->setChecked(controller_->get_gain_state().amp_enable);
    connect(amp_check_box, &QCheckBox::stateChanged, [this](int state) {
        controller_->set_amp_enable(state == Qt::Checked);
        update_total_gain();
    });
    control_layout->addRow(amp_check_box);

    auto* lna_label = new QLabel("LNA Gain");
    auto* lna_slider = new QSlider(Qt::Horizontal);
    lna_slider->setRange(0, gain_limits::LNA_MAX / gain_limits::LNA_STEP);
    lna_slider->setValue(controller_->get_gain_state().lna_gain / gain_limits::LNA_STEP);
    lna_slider->setTickInterval(1);
    lna_slider->setSingleStep(1);
    connect(lna_slider, &QSlider::valueChanged, [this](int value) {
        controller_->set_lna_gain(value * gain_limits::LNA_STEP);
        update_total_gain();
    });
    control_layout->addRow(lna_label, lna_slider);

    auto* vga_label = new QLabel("VGA Gain");
    auto* vga_slider = new QSlider(Qt::Horizontal);
    vga_slider->setRange(0, gain_limits::VGA_MAX / gain_limits::VGA_STEP);
    vga_slider->setValue(controller_->get_gain_state().vga_gain / gain_limits::VGA_STEP);
    vga_slider->setTickInterval(1);
    vga_slider->setSingleStep(1);
    connect(vga_slider, &QSlider::valueChanged, [this](int value) {
        controller_->set_vga_gain(value * gain_limits::VGA_STEP);
        update_total_gain();
    });
    control_layout->addRow(vga_label, vga_slider);

    auto* total_gain_label = new QLabel("Total Gain:");
    total_gain_field_ = new QLineEdit();
    total_gain_field_->setReadOnly(true);
    control_layout->addRow(total_gain_label, total_gain_field_);
    update_total_gain();

    main_layout->addLayout(control_layout);

    custom_plot_ = new QwtPlot();
    custom_plot_->setTitle("Frequency Sweep");
    custom_plot_->setAxisTitle(QwtPlot::xBottom, "Frequency (MHz)");
    custom_plot_->setAxisTitle(QwtPlot::yLeft, "Power (dB)");
    custom_plot_->setAxisScale(QwtPlot::yLeft, -110, 20);

    curve_ = new QwtPlotCurve();
    curve_->setTitle("Sweep Data");
    curve_->attach(custom_plot_);

    main_layout->addWidget(custom_plot_);

    color_plot_ = new QwtPlot();

    color_map_ = new QwtPlotSpectrogram();
    color_map_->setColorMap(new ThermalColorMap());
    color_map_->attach(color_plot_);

    main_layout->addWidget(color_plot_);

    setCentralWidget(central_widget);

    controller_->set_fft_callback([this](const FFTSweepData& data) {
        QMetaObject::invokeMethod(this, [this, data]() { update_plot(data); }, Qt::QueuedConnection);
    });
}

void MainWindow::update_plot(const FFTSweepData& data) {
    if (!dataset_spectrum_.is_initialized()) {
        dataset_spectrum_ = DatasetSpectrum(data.bin_width_hz, data.freq_ranges_mhz);

        custom_plot_->setAxisScale(QwtPlot::xBottom, data.freq_ranges_mhz.front(), data.freq_ranges_mhz.back());

        raster_data_ = new WaterfallRasterData(
            COLOR_MAP_SAMPLES,
            dataset_spectrum_.get_total_num_datapoints(),
            data.bin_width_hz,
            -90);

        raster_data_->setInterval(Qt::ZAxis, QwtInterval(-90, -25));
        color_plot_->setAxisScale(QwtPlot::xBottom, 0, dataset_spectrum_.get_total_num_datapoints());

        color_map_->setData(raster_data_);
    }

    dataset_spectrum_.add_new_data(data.band_lower.start_hz, data.band_lower.end_hz, data.band_lower.power_db);
    dataset_spectrum_.add_new_data(data.band_upper.start_hz, data.band_upper.end_hz, data.band_upper.power_db);

    constexpr uint64_t MHZ_TO_HZ = 1'000'000ULL;
    if (data.band_lower.start_hz == static_cast<uint64_t>(data.freq_ranges_mhz.front()) * MHZ_TO_HZ) {
        const QVector<double> freq_vec = dataset_spectrum_.get_frequency_array();
        const QVector<double> pwr_vec = dataset_spectrum_.get_power_array();

        curve_->setSamples(freq_vec, pwr_vec);
        custom_plot_->replot();

        raster_data_->addRow(dataset_spectrum_.get_spectrum());
        color_plot_->replot();
    }
}

void MainWindow::update_total_gain() {
    total_gain_field_->setText(QString::number(controller_->get_total_gain()) + " dB");
}
