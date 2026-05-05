#pragma once

#include <QImage>
#include <QVector>
#include <QVector3D>

namespace ikclut {

class Lut3D {
public:
    Lut3D() = default;
    explicit Lut3D(int size);

    static Lut3D identity(int size);

    bool isValid() const;
    int size() const;
    int valueCount() const;

    QVector3D value(int r, int g, int b) const;
    void setValue(int r, int g, int b, const QVector3D& value);
    QVector3D sample(const QVector3D& input) const;
    Lut3D resampled(int targetSize) const;
    QImage toIkClutImage() const;

private:
    int index(int r, int g, int b) const;

    int m_size = 0;
    QVector<QVector3D> m_values;
};

} // namespace ikclut

