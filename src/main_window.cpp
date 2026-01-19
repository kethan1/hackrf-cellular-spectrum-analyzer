#include "main_window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMetaObject>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>
#include <iostream>

#include "thermal_color_map.hpp"

MainWindow::MainWindow(HackRFController* ctrl, QWidget* parent)
    : QMainWindow(parent), controller_(ctrl) {
    auto* central_widget = new QWidget(this);
    auto* main_layout = new QHBoxLayout(central_widget);

    auto* splitter = new QSplitter(Qt::Horizontal, central_widget);

    // Left side plots
    auto* plot_widget = new QWidget();
    auto* plot_layout = new QVBoxLayout(plot_widget);
    plot_layout->setContentsMargins(0, 0, 0, 0);

    custom_plot_ = new QwtPlot();
    custom_plot_->setTitle("Frequency Sweep");
    custom_plot_->setAxisTitle(QwtPlot::xBottom, "Frequency (MHz)");
    custom_plot_->setAxisTitle(QwtPlot::yLeft, "Power (dB)");
    custom_plot_->setAxisScale(QwtPlot::yLeft, -110, 20);

    curve_ = new QwtPlotCurve();
    curve_->setTitle("Sweep Data");
    curve_->attach(custom_plot_);

    plot_layout->addWidget(custom_plot_);

    color_plot_ = new QwtPlot();

    color_map_ = new QwtPlotSpectrogram();
    color_map_->setColorMap(new ThermalColorMap());
    color_map_->attach(color_plot_);

    plot_layout->addWidget(color_plot_);

    // Right side sidebar
    auto* sidebar = new QWidget();
    sidebar->setMinimumWidth(250);
    sidebar->setMaximumWidth(350);
    setup_sidebar(sidebar);

    splitter->addWidget(plot_widget);
    splitter->addWidget(sidebar);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);

    main_layout->addWidget(splitter);
    setCentralWidget(central_widget);

    controller_->set_fft_callback([this](const FFTSweepData& data) {
        QMetaObject::invokeMethod(this, [this, data]() { update_plot(data); }, Qt::QueuedConnection);
    });

    refresh_range_list();
}

void MainWindow::setup_sidebar(QWidget* sidebar) {
    auto* sidebar_layout = new QVBoxLayout(sidebar);

    // Gain Controls Group
    auto* gain_group = new QGroupBox("Gain Controls");
    auto* gain_layout = new QVBoxLayout(gain_group);

    // AMP Enable
    auto* amp_check_box = new QCheckBox("AMP (+14 dB)");
    amp_check_box->setChecked(controller_->get_gain_state().get_amp_enable());
    connect(amp_check_box, &QCheckBox::stateChanged, [this](int state) {
        controller_->set_amp_enable(state == Qt::Checked);
        update_total_gain();
    });
    gain_layout->addWidget(amp_check_box);

    // LNA Gain
    auto* lna_widget = new QWidget();
    auto* lna_layout = new QFormLayout(lna_widget);
    lna_layout->setContentsMargins(0, 0, 0, 0);

    auto* lna_slider = new QSlider(Qt::Horizontal);
    lna_slider->setRange(0, hackrf_hardware::LNA_MAX / hackrf_hardware::LNA_STEP);
    lna_slider->setValue(controller_->get_gain_state().get_lna_gain() / hackrf_hardware::LNA_STEP);
    lna_slider->setTickInterval(1);
    lna_slider->setSingleStep(1);
    lna_slider->setTickPosition(QSlider::TicksBelow);

    auto* lna_value_label = new QLabel(QString::number(controller_->get_gain_state().get_lna_gain()) + " dB");
    connect(lna_slider, &QSlider::valueChanged, [this, lna_value_label](int value) {
        int gain = value * hackrf_hardware::LNA_STEP;
        controller_->set_lna_gain(gain);
        lna_value_label->setText(QString::number(gain) + " dB");
        update_total_gain();
    });

    lna_layout->addRow("LNA Gain:", lna_slider);
    lna_layout->addRow("", lna_value_label);
    gain_layout->addWidget(lna_widget);

    // VGA Gain
    auto* vga_widget = new QWidget();
    auto* vga_layout = new QFormLayout(vga_widget);
    vga_layout->setContentsMargins(0, 0, 0, 0);

    auto* vga_slider = new QSlider(Qt::Horizontal);
    vga_slider->setRange(0, hackrf_hardware::VGA_MAX / hackrf_hardware::VGA_STEP);
    vga_slider->setValue(controller_->get_gain_state().get_vga_gain() / hackrf_hardware::VGA_STEP);
    vga_slider->setTickInterval(1);
    vga_slider->setSingleStep(1);
    vga_slider->setTickPosition(QSlider::TicksBelow);

    auto* vga_value_label = new QLabel(QString::number(controller_->get_gain_state().get_vga_gain()) + " dB");
    connect(vga_slider, &QSlider::valueChanged, [this, vga_value_label](int value) {
        int gain = value * hackrf_hardware::VGA_STEP;
        controller_->set_vga_gain(gain);
        vga_value_label->setText(QString::number(gain) + " dB");
        update_total_gain();
    });

    vga_layout->addRow("VGA Gain:", vga_slider);
    vga_layout->addRow("", vga_value_label);
    gain_layout->addWidget(vga_widget);

    // Total Gain
    auto* total_widget = new QWidget();
    auto* total_layout = new QFormLayout(total_widget);
    total_layout->setContentsMargins(0, 0, 0, 0);

    total_gain_field_ = new QLineEdit();
    total_gain_field_->setReadOnly(true);
    total_gain_field_->setAlignment(Qt::AlignCenter);
    total_layout->addRow("Total Gain:", total_gain_field_);
    gain_layout->addWidget(total_widget);
    update_total_gain();

    sidebar_layout->addWidget(gain_group);

    // Scan Ranges Group
    auto* ranges_group = new QGroupBox("Scan Ranges");
    auto* ranges_layout = new QVBoxLayout(ranges_group);

    range_list_ = new QListWidget();
    range_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    ranges_layout->addWidget(range_list_);

    // Add range controls
    auto* add_range_widget = new QWidget();
    auto* add_range_layout = new QFormLayout(add_range_widget);
    add_range_layout->setContentsMargins(0, 0, 0, 0);

    start_freq_spin_ = new QSpinBox();
    start_freq_spin_->setRange(FREQ_MIN_MHZ, FREQ_MAX_MHZ);
    start_freq_spin_->setValue(FREQ_MIN_MHZ);
    start_freq_spin_->setSuffix(" MHz");
    add_range_layout->addRow("Start:", start_freq_spin_);

    end_freq_spin_ = new QSpinBox();
    end_freq_spin_->setRange(FREQ_MIN_MHZ, FREQ_MAX_MHZ);
    end_freq_spin_->setValue(FREQ_MAX_MHZ);
    end_freq_spin_->setSuffix(" MHz");
    add_range_layout->addRow("End:", end_freq_spin_);

    ranges_layout->addWidget(add_range_widget);

    // Buttons
    auto* buttons_widget = new QWidget();
    auto* buttons_layout = new QHBoxLayout(buttons_widget);
    buttons_layout->setContentsMargins(0, 0, 0, 0);

    add_range_btn_ = new QPushButton("Add");
    connect(add_range_btn_, &QPushButton::clicked, this, &MainWindow::add_scan_range);
    buttons_layout->addWidget(add_range_btn_);

    remove_range_btn_ = new QPushButton("Remove");
    connect(remove_range_btn_, &QPushButton::clicked, this, &MainWindow::remove_selected_range);
    buttons_layout->addWidget(remove_range_btn_);

    ranges_layout->addWidget(buttons_widget);

    apply_ranges_btn_ = new QPushButton("Apply Ranges");
    connect(apply_ranges_btn_, &QPushButton::clicked, this, &MainWindow::apply_scan_ranges);
    ranges_layout->addWidget(apply_ranges_btn_);

    sidebar_layout->addWidget(ranges_group);

    sidebar_layout->addStretch();
}

void MainWindow::refresh_range_list() {
    range_list_->clear();

    const auto ranges = controller_->get_scan_ranges();
    for (const auto& range : ranges) {
        QString item_text = QString("%1 - %2 MHz")
                                .arg(range.start_mhz)
                                .arg(range.end_mhz);
        range_list_->addItem(item_text);
    }
}

void MainWindow::add_scan_range() {
    const uint16_t start = static_cast<uint16_t>(start_freq_spin_->value());
    const uint16_t end = static_cast<uint16_t>(end_freq_spin_->value());

    if (start >= end) {
        QMessageBox::warning(this, "Invalid Range",
                             "Start frequency must be less than end frequency.");
        return;
    }

    auto ranges = controller_->get_scan_ranges();
    ranges.push_back({start, end});

    if (controller_->set_scan_ranges(ranges)) {
        refresh_range_list();
    } else {
        QMessageBox::warning(this, "Error",
                             "Failed to add scan range. Check frequency limits (1-6000 MHz).");
    }
}

void MainWindow::remove_selected_range() {
    const int row = range_list_->currentRow();
    if (row < 0) {
        QMessageBox::information(this, "No Selection", "Please select a range to remove.");
        return;
    }

    auto ranges = controller_->get_scan_ranges();
    if (ranges.size() <= 1) {
        QMessageBox::warning(this, "Cannot Remove", "At least one scan range is required.");
        return;
    }

    ranges.erase(ranges.begin() + row);

    if (controller_->set_scan_ranges(ranges)) {
        refresh_range_list();
    }
}

void MainWindow::apply_scan_ranges() {
    dataset_spectrum_ = DatasetSpectrum();

    controller_->restart_sweep();

    QMessageBox::information(this, "Ranges Applied", "Scan ranges have been applied. The sweep will restart.");
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
    if (data.band_lower.start_hz == data.freq_ranges_mhz.front() * MHZ_TO_HZ) {
        const QVector<double> freq_vec = dataset_spectrum_.get_frequency_array();
        const QVector<double> pwr_vec = dataset_spectrum_.get_power_array();

        curve_->setSamples(freq_vec, pwr_vec);
        custom_plot_->replot();

        raster_data_->addRow(dataset_spectrum_.get_spectrum());
        color_plot_->replot();
    }
}

void MainWindow::update_total_gain() {
    total_gain_field_->setText(QString::number(controller_->get_gain_state().total_gain()) + " dB");
}
