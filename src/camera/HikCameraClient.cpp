#include "camera/HikCameraClient.h"

#include "camera/HikCameraConfig.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4828)
#endif
#include <HCNetSDK.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <QByteArray>
#include <QMetaObject>
#include <QPointer>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <functional>
#include <utility>

namespace {

QString sdkErrorMessage(const QString &operation)
{
    return QStringLiteral("%1失败（HCNetSDK 错误码 %2）")
        .arg(operation)
        .arg(NET_DVR_GetLastError());
}

template<std::size_t Size>
void copySdkText(char (&target)[Size], const QString &source)
{
    const QByteArray bytes = source.toUtf8();
    std::memset(target, 0, Size);
    std::memcpy(target,
                bytes.constData(),
                std::min<std::size_t>(Size - 1, std::size_t(bytes.size())));
}

} // namespace

class HikCameraWorker : public QObject
{
public:
    using StatusReporter = std::function<void(bool, const QString &)>;
    using MessageReporter = std::function<void(const QString &)>;
    using ParametersReporter = std::function<void(const HikCameraParameters &)>;

    HikCameraWorker(StatusReporter statusReporter,
                    MessageReporter infoReporter,
                    MessageReporter errorReporter,
                    ParametersReporter parametersReporter)
        : m_statusReporter(std::move(statusReporter))
        , m_infoReporter(std::move(infoReporter))
        , m_errorReporter(std::move(errorReporter))
        , m_parametersReporter(std::move(parametersReporter))
    {
    }

    void startPreview(const HikCameraConfig &config, WId targetWindow)
    {
        stopPreview();
        m_config = config;
        m_targetWindow = targetWindow;

        if (!initializeSdk(config)) {
            return;
        }

        NET_DVR_USER_LOGIN_INFO loginInfo{};
        NET_DVR_DEVICEINFO_V40 deviceInfo{};
        copySdkText(loginInfo.sDeviceAddress, config.address);
        copySdkText(loginInfo.sUserName, config.username);
        copySdkText(loginInfo.sPassword, config.password);
        loginInfo.wPort = WORD(config.port);
        loginInfo.bUseAsynLogin = FALSE;
        loginInfo.byLoginMode = 0;

        m_userId = NET_DVR_Login_V40(&loginInfo, &deviceInfo);
        if (m_userId < 0) {
            reportFailure(sdkErrorMessage(QStringLiteral("登录海康相机")));
            return;
        }

        if (!openPreview()) {
            const QString message = sdkErrorMessage(QStringLiteral("启动海康实时预览"));
            NET_DVR_Logout(m_userId);
            m_userId = -1;
            reportFailure(message);
            return;
        }

        m_statusReporter(true, QStringLiteral("实时画面已连接，正在读取预览参数"));
        m_infoReporter(QStringLiteral("海康网络相机实时预览已启动：%1:%2，通道 %3，%4")
                           .arg(config.address)
                           .arg(config.port)
                           .arg(config.channel)
                           .arg(config.streamType == 0 ? QStringLiteral("主码流")
                                                       : QStringLiteral("子码流")));
        readPreviewParameters();
    }

    void stopPreview()
    {
        closePreview();
        if (m_userId >= 0) {
            NET_DVR_Logout(m_userId);
            m_userId = -1;
        }
    }

    void updatePreviewParameters(const HikCameraParameters &parameters)
    {
        if (m_userId < 0 || m_previewHandle.load() < 0) {
            m_errorReporter(QStringLiteral("海康相机未连接，不能修改预览参数"));
            return;
        }

        bool updated = false;
        bool streamChanged = false;
        if (parameters.streamTypeValid
            && (parameters.streamType == 0 || parameters.streamType == 1)
            && parameters.streamType != m_config.streamType) {
            const int previousStreamType = m_config.streamType;
            closePreview();
            m_config.streamType = parameters.streamType;
            if (openPreview()) {
                updated = true;
                streamChanged = true;
                m_statusReporter(true, QStringLiteral("预览码流已切换"));
                QString persistenceError;
                if (!m_config.save(HikCameraConfig::defaultFilePath(), &persistenceError)) {
                    m_errorReporter(QStringLiteral("码流已切换，但本地配置未保存：%1").arg(persistenceError));
                }
            } else {
                m_errorReporter(sdkErrorMessage(QStringLiteral("切换海康预览码流")));
                m_config.streamType = previousStreamType;
                if (openPreview()) {
                    m_statusReporter(true, QStringLiteral("已恢复原预览码流"));
                } else {
                    reportFailure(sdkErrorMessage(QStringLiteral("恢复原预览码流")));
                }
                readPreviewParameters();
                return;
            }
        }

        // 码流切换后先回读新码流帧率，不能把旧码流的 UI 快照写入新码流。
        if (parameters.frameRateValid && !streamChanged) {
            NET_DVR_COMPRESSIONCFG_V30 config{};
            config.dwSize = sizeof(config);
            DWORD returnedBytes = 0;
            if (NET_DVR_GetDVRConfig(m_userId, NET_DVR_GET_COMPRESSCFG_V30,
                                     m_config.channel, &config, sizeof(config), &returnedBytes)) {
                auto &stream = m_config.streamType == 0 ? config.struNormHighRecordPara
                                                        : config.struNetPara;
                if (stream.dwVideoFrameRate != DWORD(parameters.frameRateCode)) {
                    stream.dwVideoFrameRate = DWORD(parameters.frameRateCode);
                    if (NET_DVR_SetDVRConfig(m_userId, NET_DVR_SET_COMPRESSCFG_V30,
                                             m_config.channel, &config, sizeof(config))) {
                        updated = true;
                    } else {
                        m_errorReporter(sdkErrorMessage(QStringLiteral("设置海康预览帧率")));
                    }
                }
            } else {
                m_errorReporter(sdkErrorMessage(QStringLiteral("读取海康码流配置")));
            }
        }

        if (parameters.imageParametersValid) {
            NET_DVR_CAMERAPARAMCFG config{};
            config.dwSize = sizeof(config);
            DWORD returnedBytes = 0;
            if (NET_DVR_GetDVRConfig(m_userId, NET_DVR_GET_CCDPARAMCFG,
                                     m_config.channel, &config, sizeof(config), &returnedBytes)) {
                config.struVideoEffect.byBrightnessLevel = BYTE(std::clamp(parameters.brightness, 0, 100));
                config.struVideoEffect.byContrastLevel = BYTE(std::clamp(parameters.contrast, 0, 100));
                if (parameters.exposureValid) {
                    config.struExposure.byExposureMode = BYTE(parameters.exposureMode == 0 ? 0 : 1);
                    config.struExposure.dwVideoExposureSet = DWORD(
                        std::clamp(parameters.exposureTimeUs, 20, 1000000));
                }
                if (parameters.highlightValid) {
                    constexpr BYTE highlightMask = BYTE(1 << 2);
                    if (parameters.highlightEnabled) {
                        config.struVideoEffect.byEnableFunc |= highlightMask;
                    } else {
                        config.struVideoEffect.byEnableFunc &= BYTE(~highlightMask);
                    }
                    config.struVideoEffect.byLightInhibitLevel = BYTE(
                        std::clamp(parameters.highlightLevel, 1, 3));
                }
                if (NET_DVR_SetDVRConfig(m_userId, NET_DVR_SET_CCDPARAMCFG,
                                         m_config.channel, &config, sizeof(config))) {
                    updated = true;
                } else {
                    m_errorReporter(sdkErrorMessage(QStringLiteral("设置海康图像参数")));
                }
            } else {
                m_errorReporter(sdkErrorMessage(QStringLiteral("读取海康图像配置")));
            }
        }

        // 无论写入成功或失败，都以设备回读值刷新 UI，避免显示未生效的目标值。
        readPreviewParameters();
        if (updated) {
            m_infoReporter(QStringLiteral("海康预览参数已更新并重新读取"));
        }
    }

    void readPreviewParameters()
    {
        if (m_userId < 0) {
            m_parametersReporter(HikCameraParameters{});
            return;
        }

        HikCameraParameters parameters;
        parameters.streamTypeValid = true;
        parameters.streamType = m_config.streamType;

        NET_DVR_COMPRESSIONCFG_V30 compression{};
        compression.dwSize = sizeof(compression);
        DWORD returnedBytes = 0;
        if (NET_DVR_GetDVRConfig(m_userId, NET_DVR_GET_COMPRESSCFG_V30,
                                 m_config.channel, &compression, sizeof(compression), &returnedBytes)) {
            const auto &stream = m_config.streamType == 0 ? compression.struNormHighRecordPara
                                                          : compression.struNetPara;
            parameters.frameRateValid = true;
            parameters.frameRateCode = int(stream.dwVideoFrameRate);
        }

        NET_DVR_CAMERAPARAMCFG camera{};
        camera.dwSize = sizeof(camera);
        returnedBytes = 0;
        if (NET_DVR_GetDVRConfig(m_userId, NET_DVR_GET_CCDPARAMCFG,
                                 m_config.channel, &camera, sizeof(camera), &returnedBytes)) {
            parameters.imageParametersValid = true;
            parameters.brightness = camera.struVideoEffect.byBrightnessLevel;
            parameters.contrast = camera.struVideoEffect.byContrastLevel;
            parameters.exposureValid = camera.struExposure.byExposureMode <= 1
                && camera.struExposure.dwVideoExposureSet >= 20
                && camera.struExposure.dwVideoExposureSet <= 1000000;
            parameters.exposureMode = camera.struExposure.byExposureMode;
            parameters.exposureTimeUs = int(camera.struExposure.dwVideoExposureSet);
            parameters.highlightValid = camera.struVideoEffect.byLightInhibitLevel >= 1
                && camera.struVideoEffect.byLightInhibitLevel <= 3;
            parameters.highlightEnabled = (camera.struVideoEffect.byEnableFunc & BYTE(1 << 2)) != 0;
            parameters.highlightLevel = camera.struVideoEffect.byLightInhibitLevel;
        }
        m_parametersReporter(parameters);
    }

    void shutdown()
    {
        stopPreview();
        if (m_sdkInitialized) {
            NET_DVR_Cleanup();
            m_sdkInitialized = false;
        }
    }

private:
    bool initializeSdk(const HikCameraConfig &config)
    {
        if (!m_sdkInitialized) {
            if (!NET_DVR_Init()) {
                reportFailure(sdkErrorMessage(QStringLiteral("初始化 HCNetSDK")));
                return false;
            }
            m_sdkInitialized = true;
            NET_DVR_SetExceptionCallBack_V30(0, nullptr, &HikCameraWorker::exceptionCallback, this);
        }

        NET_DVR_SetConnectTime(DWORD(config.loginTimeoutMs), 1);
        NET_DVR_SetReconnect(DWORD(config.reconnectIntervalMs), TRUE);
        return true;
    }

    bool openPreview()
    {
        NET_DVR_PREVIEWINFO previewInfo{};
        previewInfo.lChannel = m_config.channel;
        previewInfo.dwStreamType = DWORD(m_config.streamType);
        previewInfo.dwLinkMode = DWORD(m_config.linkMode);
        previewInfo.hPlayWnd = reinterpret_cast<HWND>(m_targetWindow);
        previewInfo.bBlocked = FALSE;
        previewInfo.dwDisplayBufNum = 1;

        const LONG previewHandle = NET_DVR_RealPlay_V40(
            m_userId, &previewInfo, nullptr, this);
        if (previewHandle < 0) {
            return false;
        }
        m_previewHandle.store(previewHandle);
        return true;
    }

    void closePreview()
    {
        const LONG previewHandle = m_previewHandle.exchange(-1);
        if (previewHandle >= 0) {
            NET_DVR_StopRealPlay(previewHandle);
        }
    }

    void reportFailure(const QString &message)
    {
        m_statusReporter(false, message);
        m_errorReporter(message);
    }

    static void CALLBACK exceptionCallback(DWORD type,
                                           LONG,
                                           LONG handle,
                                           void *userData)
    {
        auto *worker = static_cast<HikCameraWorker *>(userData);
        if (!worker || handle != worker->m_previewHandle.load()) {
            return;
        }

        if (type == EXCEPTION_RECONNECT || type == EXCEPTION_PREVIEW) {
            const QString message = QStringLiteral("海康实时画面连接中断，正在自动重连");
            worker->m_statusReporter(false, message);
            worker->m_errorReporter(message);
        } else if (type == PREVIEW_RECONNECTSUCCESS) {
            const QString message = QStringLiteral("海康实时画面连接已恢复，正在重新读取参数");
            worker->m_statusReporter(true, message);
            worker->m_infoReporter(message);
            QMetaObject::invokeMethod(worker, [worker]() {
                worker->readPreviewParameters();
            }, Qt::QueuedConnection);
        }
    }

    StatusReporter m_statusReporter;
    MessageReporter m_infoReporter;
    MessageReporter m_errorReporter;
    ParametersReporter m_parametersReporter;
    HikCameraConfig m_config;
    WId m_targetWindow{0};
    LONG m_userId{-1};
    std::atomic<LONG> m_previewHandle{-1};
    bool m_sdkInitialized{false};
};

HikCameraClient::HikCameraClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<HikCameraParameters>("HikCameraParameters");
    const QPointer<HikCameraClient> guard(this);
    auto statusReporter = [guard](bool connected, const QString &message) {
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, connected, message]() {
            if (guard) {
                emit guard->connectionStatusChanged(connected, message);
            }
        }, Qt::QueuedConnection);
    };
    auto infoReporter = [guard](const QString &message) {
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, message]() {
            if (guard) {
                emit guard->infoMessage(message);
            }
        }, Qt::QueuedConnection);
    };
    auto errorReporter = [guard](const QString &message) {
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, message]() {
            if (guard) {
                emit guard->errorMessage(message);
            }
        }, Qt::QueuedConnection);
    };
    auto parametersReporter = [guard](const HikCameraParameters &parameters) {
        if (!guard) {
            return;
        }
        QMetaObject::invokeMethod(guard, [guard, parameters]() {
            if (guard) {
                emit guard->parametersUpdated(parameters);
            }
        }, Qt::QueuedConnection);
    };

    m_worker = new HikCameraWorker(statusReporter, infoReporter, errorReporter, parametersReporter);
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_workerThread.setObjectName(QStringLiteral("HikCameraWorker"));
    m_workerThread.start();
}

HikCameraClient::~HikCameraClient()
{
    shutdown();
}

void HikCameraClient::startPreview(WId targetWindow)
{
    if (m_shutdown || !m_worker) {
        return;
    }

    QString loadError;
    const HikCameraConfig config = HikCameraConfig::load(
        HikCameraConfig::defaultFilePath(), &loadError);
    QString validationError;
    if (!loadError.isEmpty() || !config.validate(&validationError)) {
        const QString message = !loadError.isEmpty() ? loadError : validationError;
        emit connectionStatusChanged(false, message);
        emit errorMessage(message);
        return;
    }

    HikCameraWorker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, config, targetWindow]() {
        worker->startPreview(config, targetWindow);
    }, Qt::QueuedConnection);
}

void HikCameraClient::stopPreview()
{
    if (m_shutdown || !m_worker) {
        return;
    }
    HikCameraWorker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker]() {
        worker->stopPreview();
    }, Qt::QueuedConnection);
    emit connectionStatusChanged(false, QStringLiteral("实时预览已停止"));
}

void HikCameraClient::refreshPreviewParameters()
{
    if (m_shutdown || !m_worker) {
        return;
    }
    HikCameraWorker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker]() {
        worker->readPreviewParameters();
    }, Qt::QueuedConnection);
}

void HikCameraClient::updatePreviewParameters(const HikCameraParameters &parameters)
{
    if (m_shutdown || !m_worker) {
        return;
    }
    HikCameraWorker *worker = m_worker;
    QMetaObject::invokeMethod(worker, [worker, parameters]() {
        worker->updatePreviewParameters(parameters);
    }, Qt::QueuedConnection);
}

void HikCameraClient::shutdown()
{
    if (m_shutdown) {
        return;
    }
    m_shutdown = true;

    if (m_worker && m_workerThread.isRunning()) {
        HikCameraWorker *worker = m_worker;
        QMetaObject::invokeMethod(worker, [worker]() {
            worker->shutdown();
        }, Qt::BlockingQueuedConnection);
        m_workerThread.quit();
        m_workerThread.wait();
    }
    m_worker = nullptr;
}
