#ifndef WATERFALL_RASTER_DATA_HPP
#define WATERFALL_RASTER_DATA_HPP

#include <qwt_interval.h>
#include <qwt_matrix_raster_data.h>

#include <QVector>
#include <map>
#include <vector>

class WaterfallRasterData : public QwtMatrixRasterData {
   private:
    std::vector<double> m_data;
    int m_currentIndex;
    int m_maxRows;
    int m_cols;
    int bin_width;
    double init_value;

   public:
    WaterfallRasterData(int rows, int cols, int bin_width, double init_value);
    virtual ~WaterfallRasterData();

    void addRow(QVector<double> newRow);
    void addRow(std::map<uint64_t, float> newRow);

    virtual double value(double x, double y) const override;
};

#endif  // WATERFALL_RASTER_DATA_HPP
