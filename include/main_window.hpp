#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <vector>

#include "hackrf_controller.hpp"
#include "qcustomplot.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(HackRF_Controller *controller, QWidget *parent = nullptr);

   private:
    QCustomPlot *custom_plot;
    HackRF_Controller *controller;
    QLineEdit *total_gain_field;
    void update_plot(const std::vector<double> &x_data, const std::vector<double> &y_data);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_H
