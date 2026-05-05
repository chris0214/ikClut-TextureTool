#include "core/Lut3D.h"

#include <QtMath>

namespace ikclut {

namespace {

float clamp01(float v)
{
    return std::clamp(v, 0.0f, 1.0f);
}

int quantize8(float v)
{
    return std::clamp(static_cast<int>(std::lround(clamp01(v) * 255.0f)), 0, 255);
}

} // namespace

Lut3D::Lut3D(int size)
    : m_size(size),
      m_values(size > 0 ? size * size * size : 0)
{
}

Lut3D Lut3D::identity(int size)
{
    Lut3D lut(size);
    if (size < 2) {
        return lut;
    }
    const float denom = static_cast<float>(size - 1);
    for (int b = 0; b < size; ++b) {
        for (int g = 0; g < size; ++g) {
            for (int r = 0; r < size; ++r) {
                lut.setValue(r, g, b, QVector3D(r / denom, g / denom, b / denom));
            }
        }
    }
    return lut;
}

bool Lut3D::isValid() const
{
    return m_size >= 2 && m_values.size() == m_size * m_size * m_size;
}

int Lut3D::size() const
{
    return m_size;
}

int Lut3D::valueCount() const
{
    return m_values.size();
}

QVector3D Lut3D::value(int r, int g, int b) const
{
    if (!isValid()) {
        return QVector3D();
    }
    r = std::clamp(r, 0, m_size - 1);
    g = std::clamp(g, 0, m_size - 1);
    b = std::clamp(b, 0, m_size - 1);
    return m_values.at(index(r, g, b));
}

void Lut3D::setValue(int r, int g, int b, const QVector3D& value)
{
    if (!isValid()) {
        return;
    }
    m_values[index(r, g, b)] = QVector3D(clamp01(value.x()), clamp01(value.y()), clamp01(value.z()));
}

QVector3D Lut3D::sample(const QVector3D& input) const
{
    if (!isValid()) {
        return QVector3D(clamp01(input.x()), clamp01(input.y()), clamp01(input.z()));
    }

    const float x = clamp01(input.x()) * static_cast<float>(m_size - 1);
    const float y = clamp01(input.y()) * static_cast<float>(m_size - 1);
    const float z = clamp01(input.z()) * static_cast<float>(m_size - 1);

    const int x0 = std::clamp(static_cast<int>(std::floor(x)), 0, m_size - 1);
    const int y0 = std::clamp(static_cast<int>(std::floor(y)), 0, m_size - 1);
    const int z0 = std::clamp(static_cast<int>(std::floor(z)), 0, m_size - 1);
    const int x1 = std::min(x0 + 1, m_size - 1);
    const int y1 = std::min(y0 + 1, m_size - 1);
    const int z1 = std::min(z0 + 1, m_size - 1);

    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const float tz = z - static_cast<float>(z0);

    const QVector3D c000 = value(x0, y0, z0);
    const QVector3D c100 = value(x1, y0, z0);
    const QVector3D c010 = value(x0, y1, z0);
    const QVector3D c110 = value(x1, y1, z0);
    const QVector3D c001 = value(x0, y0, z1);
    const QVector3D c101 = value(x1, y0, z1);
    const QVector3D c011 = value(x0, y1, z1);
    const QVector3D c111 = value(x1, y1, z1);

    const QVector3D c00 = c000 * (1.0f - tx) + c100 * tx;
    const QVector3D c10 = c010 * (1.0f - tx) + c110 * tx;
    const QVector3D c01 = c001 * (1.0f - tx) + c101 * tx;
    const QVector3D c11 = c011 * (1.0f - tx) + c111 * tx;
    const QVector3D c0 = c00 * (1.0f - ty) + c10 * ty;
    const QVector3D c1 = c01 * (1.0f - ty) + c11 * ty;
    return c0 * (1.0f - tz) + c1 * tz;
}

Lut3D Lut3D::resampled(int targetSize) const
{
    Lut3D output(targetSize);
    if (!output.isValid()) {
        return output;
    }

    const float denom = static_cast<float>(targetSize - 1);
    for (int b = 0; b < targetSize; ++b) {
        for (int g = 0; g < targetSize; ++g) {
            for (int r = 0; r < targetSize; ++r) {
                output.setValue(r, g, b, sample(QVector3D(r / denom, g / denom, b / denom)));
            }
        }
    }
    return output;
}

QImage Lut3D::toIkClutImage() const
{
    if (!isValid()) {
        return QImage();
    }

    QImage image(m_size * m_size, m_size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);

    for (int b = 0; b < m_size; ++b) {
        for (int g = 0; g < m_size; ++g) {
            for (int r = 0; r < m_size; ++r) {
                const QVector3D v = value(r, g, b);
                image.setPixel(b * m_size + r, g, qRgba(quantize8(v.x()), quantize8(v.y()), quantize8(v.z()), 255));
            }
        }
    }
    return image;
}

int Lut3D::index(int r, int g, int b) const
{
    return (b * m_size + g) * m_size + r;
}

} // namespace ikclut

