#ifndef HIKCAMERAPARAMETERS_H
#define HIKCAMERAPARAMETERS_H

#include <QMetaType>

// 海康实时预览参数的实测快照。valid=false 表示未连接、读取失败或设备不支持，
// UI 不得用默认值代替真实设备值。
struct HikCameraParameters
{
    bool streamTypeValid{false};
    int streamType{1};              // 0 主码流，1 子码流

    bool frameRateValid{false};
    int frameRateCode{0};           // HCNetSDK dwVideoFrameRate 原始枚举

    bool imageParametersValid{false};
    int brightness{0};              // 0~100
    int contrast{0};                // 0~100

    bool exposureValid{false};
    int exposureMode{1};            // 0 手动，1 自动
    int exposureTimeUs{0};          // 20~1000000 us

    bool highlightValid{false};
    bool highlightEnabled{false};
    int highlightLevel{1};          // 1~3
};

Q_DECLARE_METATYPE(HikCameraParameters)

#endif // HIKCAMERAPARAMETERS_H
