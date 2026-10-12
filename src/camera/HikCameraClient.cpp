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

    HikCameraWorker(StatusReporter statusReporter,
                    MessageReporter infoReporter,
                    MessageReporter errorReporter)
        : m_statusReporter(std::move(statusReporter))
        , m_infoReporter(std::move(infoReporter))
        , m_errorReporter(std::move(errorReporter))
    {
    }

    void startPreview(const HikCameraConfig &config, WId targetWindow)
    {
        stopPreview();

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
            const QString message = sdkErrorMessage(QStringLiteral("登录海康相机"));
            reportFailure(message);
            return;
        }

        NET_DVR_PREVIEWINFO previewInfo{};
        previewInfo.lChannel = config.channel;
        previewInfo.dwStreamType = DWORD(config.streamType);
        previewInfo.dwLinkMode = DWORD(config.linkMode);
        previewInfo.hPlayWnd = reinterpret_cast<HWND>(targetWindow);
        previewInfo.bBlocked = FALSE;
        previewInfo.dwDisplayBufNum = 1;

        const LONG previewHandle = NET_DVR_RealPlay_V40(
            m_userId, &previewInfo, nullptr, this);
        if (previewHandle < 0) {
            const QString message = sdkErrorMessage(QStringLiteral("启动海康实时预览"));
            NET_DVR_Logout(m_userId);
            m_userId = -1;
            reportFailure(message);
            return;
        }
        m_previewHandle.store(previewHandle);

        m_statusReporter(true, QStringLiteral("实时画面已连接"));
        m_infoReporter(QStringLiteral("海康网络相机实时预览已启动：%1:%2，通道 %3，%4")
                           .arg(config.address)
                           .arg(config.port)
                           .arg(config.channel)
                           .arg(config.streamType == 0 ? QStringLiteral("主码流")
                                                       : QStringLiteral("子码流")));
    }

    void stopPreview()
    {
        const LONG previewHandle = m_previewHandle.exchange(-1);
        if (previewHandle >= 0) {
            NET_DVR_StopRealPlay(previewHandle);
        }
        if (m_userId >= 0) {
            NET_DVR_Logout(m_userId);
            m_userId = -1;
        }
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
            const QString message = QStringLiteral("海康实时画面连接已恢复");
            worker->m_statusReporter(true, message);
            worker->m_infoReporter(message);
        }
    }

    StatusReporter m_statusReporter;
    MessageReporter m_infoReporter;
    MessageReporter m_errorReporter;
    LONG m_userId{-1};
    std::atomic<LONG> m_previewHandle{-1};
    bool m_sdkInitialized{false};
};

HikCameraClient::HikCameraClient(QObject *parent)
    : QObject(parent)
{
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

    m_worker = new HikCameraWorker(statusReporter, infoReporter, errorReporter);
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
