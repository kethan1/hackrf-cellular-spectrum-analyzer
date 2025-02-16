#include "main_window.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

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
    custom_plot->yAxis->setLabel("Power (dB)");
    custom_plot->yAxis->setRange(-110, 20);
    QSharedPointer<QCPAxisTickerFixed> fixed_ticker(new QCPAxisTickerFixed);
    custom_plot->yAxis->setTicker(fixed_ticker);
    fixed_ticker->setTickStep(10);

    main_layout->addWidget(custom_plot);
    setCentralWidget(central_widget);

    controller->set_fft_callback([this](const std::vector<double> &x_data, const std::vector<double> &y_data) {
        QMetaObject::invokeMethod(this, [=]() { update_plot(x_data, y_data); }, Qt::QueuedConnection);
    });
}

void MainWindow::update_plot(const std::vector<double> &x_data, const std::vector<double> &y_data) {
    custom_plot->graph(0)->setData(QVector<double>(x_data.begin(), x_data.end()),
                                   QVector<double>(y_data.begin(), y_data.end()));
    custom_plot->xAxis->rescale();
    custom_plot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::update_total_gain() {
    total_gain_field->setText(QString::number(controller->get_total_gain()) + " dB");
}
