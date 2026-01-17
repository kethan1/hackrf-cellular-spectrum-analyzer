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

constexpr int COLOR_MAP_SAMPLES = 300;

class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(HackRFController* controller, QWidget* parent = nullptr);

private:
    QwtPlot* custom_plot_ = nullptr;
    QwtPlotCurve* curve_ = nullptr;

    QwtPlot* color_plot_ = nullptr;
    QwtPlotSpectrogram* color_map_ = nullptr;
    WaterfallRasterData* raster_data_ = nullptr;

    DatasetSpectrum dataset_spectrum_;
    HackRFController* controller_ = nullptr;
    QLineEdit* total_gain_field_ = nullptr;

    void update_plot(const FFTSweepData& data);
    void update_total_gain();
};

#endif  // MAIN_WINDOW_HPP
