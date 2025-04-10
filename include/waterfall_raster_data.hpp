#ifndef WATERFALLRASTERDATA_HPP
#define WATERFALLRASTERDATA_HPP

#include <QVector>
#include <vector>
#include <qwt_matrix_raster_data.h>
#include <qwt_interval.h>

class WaterfallRasterData : public QwtMatrixRasterData
{
public:
    WaterfallRasterData(int rows, int cols, double init_value);
    virtual ~WaterfallRasterData();

    void addRow(QVector<double> newRow);

    virtual double value(double x, double y) const override;

private:
    std::vector<double> m_data;
    int m_maxRows;
    int m_cols;
    int m_currentIndex;
};

#endif // WATERFALLRASTERDATA_HPP
