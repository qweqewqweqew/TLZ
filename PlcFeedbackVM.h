#ifndef PLCFEEDBACKVM_H
#define PLCFEEDBACKVM_H

#include <QMetaType>
#include <QtGlobal>

// PLC 反馈：字段与 mz_interfaces/msg/PlcPathParams.msg 一一对应。
struct PlcPathParamsVM
{
    float xPos{0.0f};
    float yPos{0.0f};
    float zPos{0.0f};
    float xSpeed{0.0f};
    float ySpeed{0.0f};
    float zSpeed{0.0f};
    float spindleSpeed{0.0f};
    float spindleTorque{0.0f};
};

// 顶层 PLC 反馈：字段与 mz_interfaces/msg/PlcFeedback.msg 一一对应。
struct PlcFeedbackVM
{
    quint16 statusWord{0};
    quint16 completedPath{0};
    quint16 currentPath{0};
    quint16 faultCode{0};
    PlcPathParamsVM pathParams;
};

Q_DECLARE_METATYPE(PlcPathParamsVM)
Q_DECLARE_METATYPE(PlcFeedbackVM)

#endif // PLCFEEDBACKVM_H
