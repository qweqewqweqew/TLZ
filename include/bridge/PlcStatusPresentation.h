#ifndef PLCSTATUSPRESENTATION_H
#define PLCSTATUSPRESENTATION_H

#include "app/Theme.h"

#include <QString>
#include <QtGlobal>

struct PlcStatusPresentation
{
    QString text;
    QString color;
    bool critical{false};
};

inline PlcStatusPresentation plcStatusPresentation(quint16 statusWord,
                                                    quint16 faultCode)
{
    const QString unknownColor = QString::fromLatin1(Theme::kStateUnknown);
    const QString normalColor = QString::fromLatin1(Theme::kStateSuccess);
    const QString activeColor = QString::fromLatin1(Theme::kStateRunning);
    const QString attentionColor = QString::fromLatin1(Theme::kStateWarning);
    const QString criticalColor = QString::fromLatin1(Theme::kStateError);

    if (faultCode != 0) {
        return {QStringLiteral("系统故障"), criticalColor, true};
    }

    switch (statusWord) {
    case 0:
        return {QStringLiteral("无"), unknownColor, false};
    case 1:
        return {QStringLiteral("系统急停"), criticalColor, true};
    case 2:
        return {QStringLiteral("准备就绪"), normalColor, false};
    case 3:
        return {QStringLiteral("系统故障"), criticalColor, true};
    case 4:
        return {QStringLiteral("拍照完成"), normalColor, false};
    case 12:
        return {QStringLiteral("任务执行中"), activeColor, false};
    case 20:
        return {QStringLiteral("任务暂停"), attentionColor, false};
    case 30:
        return {QStringLiteral("路径请求"), activeColor, false};
    case 40:
        return {QStringLiteral("拍照请求"), activeColor, false};
    case 50:
        return {QStringLiteral("换刀中"), attentionColor, false};
    case 60:
        return {QStringLiteral("换刀完成"), normalColor, false};
    case 80:
        return {QStringLiteral("打磨完成"), normalColor, false};
    case 100:
        return {QStringLiteral("手动模式"), attentionColor, false};
    default:
        return {QStringLiteral("未知状态"), unknownColor, false};
    }
}

// 这些状态表示流程已经在进行，开始按钮应显示为暂停。空闲、暂停、完成仍显示开始。
inline bool plcStatusShowsPauseButton(quint16 statusWord)
{
    switch (statusWord) {
    case 4:  // 拍照完成
    case 12: // 任务执行中
    case 30: // 路径请求
    case 40: // 拍照请求
    case 50: // 换刀中
    case 60: // 换刀完成
        return true;
    default:
        return false;
    }
}

#endif // PLCSTATUSPRESENTATION_H
