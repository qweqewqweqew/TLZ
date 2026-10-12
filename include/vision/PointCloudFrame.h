#ifndef POINTCLOUDFRAME_H
#define POINTCLOUDFRAME_H

#include <QByteArray>
#include <QImage>
#include <QMetaType>
#include <QVector>

struct PointCloudFrame
{
    QImage rangePreview;
    QImage intensity;
    QByteArray rangeFloat32;

    quint64 frameId{0};
    quint64 taskId{0};
    quint64 timestampNs{0};
    quint32 width{0};
    quint32 height{0};
    int rangePixelFormat{-1};
    int intensityPixelFormat{-1};

    bool hasMetricGeometry{false};
    double xOffsetMm{0.0};
    double yOffsetMm{0.0};
    double xScaleMm{1.0};
    double yScaleMm{1.0};
    float missingValue{-1.0e6f};
    double encoderResolutionMm{0.0};
    QVector<qint32> encoderValues;

    bool hasFloatRange() const
    {
        if (width == 0 || height == 0) {
            return false;
        }
        const quint64 expected = quint64(width) * quint64(height) * sizeof(float);
        return quint64(rangeFloat32.size()) == expected;
    }
};

Q_DECLARE_METATYPE(PointCloudFrame)

#endif // POINTCLOUDFRAME_H
