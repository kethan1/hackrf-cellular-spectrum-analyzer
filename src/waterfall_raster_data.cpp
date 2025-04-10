#include "waterfall_raster_data.hpp"

#include <QtGlobal>
#include <vector>

WaterfallRasterData::WaterfallRasterData(int rows, int cols, double init_value)
    : m_maxRows(rows), m_cols(cols), m_currentIndex(0)
{
    m_data.resize(m_maxRows * m_cols, init_value);

    setInterval(Qt::XAxis, QwtInterval(0, m_cols));
    setInterval(Qt::YAxis, QwtInterval(0, m_maxRows));
    setInterval(Qt::ZAxis, QwtInterval(0.0, 1.0));
}

WaterfallRasterData::~WaterfallRasterData() {
    m_data.clear();
}

void WaterfallRasterData::addRow(QVector<double> newRow) {
    if (newRow.size() != m_cols) {
        return;
    }

    int start_index = m_currentIndex * m_cols;
    for (int i = 0; i < m_cols; ++i) {
        m_data[start_index + i] = newRow.at(i);
    }

    m_currentIndex = (m_currentIndex + 1) % m_maxRows;
}

double WaterfallRasterData::value(double x, double y) const {
    int col = static_cast<int>(x);
    int row = static_cast<int>(y);

    if (col < 0 || col >= m_cols)
        return 0.0;
    if (row < 0 || row >= m_maxRows)
        return 0.0;

    int actualRow = (m_currentIndex + row) % m_maxRows;

    return m_data[actualRow * m_cols + col];
}
