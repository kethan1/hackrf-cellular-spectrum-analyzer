#ifndef MAIN_WINDOW_HPP
#define MAIN_WINDOW_HPP

#include <QMainWindow>
#include <QLineEdit>
#include <vector>

#include <qwt_color_map.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_spectrogram.h>

#include "hackrf_controller.hpp"
#include "dataset_spectrum.hpp"
#include "waterfall_raster_data.hpp"

const int COLOR_MAP_SAMPLES = 1000;

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    MainWindow(HackRF_Controller *controller, QWidget *parent = nullptr);

   private:
    QwtPlot *custom_plot;
    QwtPlotCurve *curve;

    QwtPlot *color_plot;
    QwtPlotSpectrogram *color_map;
    WaterfallRasterData *raster_data;

    DatasetSpectrum dataset_spectrum;

    HackRF_Controller *controller;
    QLineEdit *total_gain_field;

    void update_plot(const hackrf_sweep_state_t *state, uint64_t current_freq);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_HPP
