#ifndef SICKCAMERAPARAMETERSVM_H
#define SICKCAMERAPARAMETERSVM_H

#include <QMetaType>
#include <QString>
#include <QtGlobal>

// 与 mz_interfaces/msg/SickCameraParameters.msg 一一对应，仅用于跨线程传递。
struct SickCameraParametersVM
{
    quint32 scanLines{0};
    double exposureTimeUs{0.0};
    QString hdrMode;
    double yResolutionMm{0.0};
};

Q_DECLARE_METATYPE(SickCameraParametersVM)

#endif // SICKCAMERAPARAMETERSVM_H
