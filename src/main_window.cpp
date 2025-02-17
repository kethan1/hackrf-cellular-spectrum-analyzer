#include "main_window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>
#include <iostream>
#include <ranges>

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

    custom_plot = new QCustomPlot();
    custom_plot->addGraph();
    custom_plot->xAxis->setLabel("Frequency (Hz)");
    custom_plot->xAxis->setRange(SWEEP_FREQ_MIN_MHZ, SWEEP_FREQ_MAX_MHZ);
    custom_plot->yAxis->setLabel("Power (dB)");
    custom_plot->yAxis->setRange(-110, 20);

    QSharedPointer<QCPAxisTickerFixed> fixed_ticker_x(new QCPAxisTickerFixed);
    custom_plot->xAxis->setTicker(fixed_ticker_x);
    fixed_ticker_x->setTickStep((SWEEP_FREQ_MAX_MHZ - SWEEP_FREQ_MIN_MHZ) / 5);

    QSharedPointer<QCPAxisTickerFixed> fixed_ticker_y(new QCPAxisTickerFixed);
    custom_plot->yAxis->setTicker(fixed_ticker_y);
    fixed_ticker_y->setTickStep(10);

    color_plot = new QCustomPlot();
    color_plot->axisRect()->setupFullAxesBox(true);

    color_map = new QCPColorMap(color_plot->xAxis, color_plot->yAxis);

    color_map->data()->setSize(color_map_width, color_map_samples);
    color_map->data()->setRange(QCPRange(SWEEP_FREQ_MIN_MHZ, SWEEP_FREQ_MAX_MHZ), QCPRange(color_map_samples, 0));

    // add a color scale:
    QCPColorScale *colorScale = new QCPColorScale(color_plot);
    color_plot->plotLayout()->addElement(0, 1, colorScale);
    colorScale->setType(QCPAxis::atRight);
    colorScale->setDataRange(QCPRange(-110, -25));
    color_map->setColorScale(colorScale);
    colorScale->axis()->setLabel("Magnetic Field Strength");

    color_map->setGradient(QCPColorGradient::gpThermal);

    QCPMarginGroup *marginGroup = new QCPMarginGroup(color_plot);
    color_plot->axisRect()->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);
    colorScale->setMarginGroup(QCP::msBottom | QCP::msTop, marginGroup);

    color_plot->rescaleAxes();

    main_layout->addWidget(custom_plot);
    main_layout->addWidget(color_plot);
    setCentralWidget(central_widget);

    controller->set_fft_callback([this](const std::vector<double> &x_data, const std::vector<double> &y_data) {
        QMetaObject::invokeMethod(this, [=]() { update_plot(x_data, y_data); }, Qt::QueuedConnection);
    });
}

void MainWindow::update_plot(const std::vector<double> &x_data, const std::vector<double> &y_data) {
    for (int i = 0; i < x_data.size(); ++i) {
        current_data[x_data[i]] = y_data[i];
    }

    custom_plot->graph(0)->setData(QVector<double>(std::views::keys(current_data).begin(), std::views::keys(current_data).end()),
                                   QVector<double>(std::views::values(current_data).begin(), std::views::values(current_data).end()));
    custom_plot->replot(QCustomPlot::rpQueuedReplot);

    if (current_data.size() != color_map_width) {
        color_map->data()->setSize(current_data.size(), color_map_samples);
        color_plot->rescaleAxes();
        color_map_width = current_data.size();
    }

    for (int i = color_map_samples - 1; i > 0; --i) {
        for (size_t j = 0; j < color_map_width; ++j) {
            color_map->data()->setCell(j, i, color_map->data()->cell(j, i - 1));
        }
    }

    int i=0;
    for (const auto &[key, value] : current_data) {
        color_map->data()->setCell(i, 0, value);
        ++i;
    }

    color_plot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::update_total_gain() {
    total_gain_field->setText(QString::number(controller->get_total_gain()) + " dB");
}
