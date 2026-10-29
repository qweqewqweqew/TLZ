#ifndef MILLINGCOMMAND_H
#define MILLINGCOMMAND_H

#include <QString>
#include <QtGlobal>

// 与 mz_interfaces/msg/MillingCommand.msg 的常量一致。
enum class MillingCommand : quint8 {
    Start = 1,
    Pause = 2,
    Stop = 3,
};

inline QString millingCommandLabel(MillingCommand command)
{
    switch (command) {
    case MillingCommand::Start:
        return QStringLiteral("启动");
    case MillingCommand::Pause:
        return QStringLiteral("暂停");
    case MillingCommand::Stop:
        return QStringLiteral("停止");
    }
    return QStringLiteral("未知");
}

#endif // MILLINGCOMMAND_H
