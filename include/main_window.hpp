#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <qwt_color_map.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_spectrogram.h>

#include <QLineEdit>
#include <QMainWindow>
#include <vector>

#include "dataset_spectrum.hpp"
#include "hackrf_controller.hpp"
#include "waterfall_raster_data.hpp"

const int COLOR_MAP_SAMPLES = 300;

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(hackrf_controller* controller, QWidget* parent = nullptr);

   private:
    QwtPlot* custom_plot;
    QwtPlotCurve* curve;

    QwtPlot* color_plot;
    QwtPlotSpectrogram* color_map;
    WaterfallRasterData* raster_data;

    DatasetSpectrum dataset_spectrum;

    hackrf_controller* controller;
    QLineEdit* total_gain_field;

    void update_plot(db_data data);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_HPP
