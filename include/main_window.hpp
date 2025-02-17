#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>
#include <map>
#include <vector>

#include <qwt_color_map.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_spectrogram.h>

#include "hackrf_controller.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(HackRF_Controller *controller, QWidget *parent = nullptr);

   private:
    QwtPlot *custom_plot;
    QwtPlotCurve *curve;

    QwtPlot *color_plot;
    QwtPlotSpectrogram *color_map;
    QwtMatrixRasterData *raster_data;

    int color_map_samples = 100;
    size_t color_map_width = 200;

    std::map<double, double> current_data;

    HackRF_Controller *controller;
    QLineEdit *total_gain_field;

    void update_plot(const std::vector<double> &x_data, const std::vector<double> &y_data);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_HPP
