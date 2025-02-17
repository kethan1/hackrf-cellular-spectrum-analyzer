#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <vector>
#include <map>

#include "hackrf_controller.hpp"
#include "qcustomplot.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(HackRF_Controller *controller, QWidget *parent = nullptr);

   private:
    QCustomPlot *custom_plot;
    QCustomPlot *color_plot;
    QCPColorMap *color_map;
    int color_map_samples = 20;
    size_t color_map_width = 200;
    HackRF_Controller *controller;
    QLineEdit *total_gain_field;

    std::map<double, double> current_data;

    void update_plot(const std::vector<double> &x_data, const std::vector<double> &y_data);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_H
