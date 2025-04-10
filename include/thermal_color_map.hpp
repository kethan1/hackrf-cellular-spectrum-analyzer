#ifndef THERMALCOLORMAP_HPP
#define THERMALCOLORMAP_HPP

#include <qwt_color_map.h>
#include <qwt_matrix_raster_data.h>
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_spectrogram.h>

class ThermalColorMap : public QwtLinearColorMap {
   public:
    ThermalColorMap();

    QRgb rgb(const QwtInterval &interval, double value) const;
};

#endif // THERMALCOLORMAP_HPP
