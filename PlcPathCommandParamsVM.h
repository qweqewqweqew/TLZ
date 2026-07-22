#ifndef PLCPATHCOMMANDPARAMSVM_H
#define PLCPATHCOMMANDPARAMSVM_H

#include <QMetaType>
#include <QtGlobal>

struct PlcPathCommandParamsVM
{
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
    float feedSpeed{0.0f};
    float feedAmount{0.0f};
    float spindleSpeed{0.0f};
    quint16 plungeCount{0};
    quint16 plungeAmount{0};
    qint16 feedDirection{0};
};

Q_DECLARE_METATYPE(PlcPathCommandParamsVM)

#endif // PLCPATHCOMMANDPARAMSVM_H
