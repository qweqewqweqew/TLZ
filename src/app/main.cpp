#include "app/mainwindow.h"
#include "auth/AuthService.h"
#include "data/Database.h"
#include "data/DatabaseBootstrap.h"
#include "data/InspectionRepository.h"
#include "app/Logger.h"

#include "ElaApplication.h"
#include "ElaMessageBar.h"
#include "ElaTheme.h"
#include <QDebug>
#include <rclcpp/rclcpp.hpp>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QThread>

#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

struct DatabaseBootstrapResult
{
    bool ready{false};
    QString error;
};

constexpr int kDatabaseBootstrapAttempts = 3;

QString executableDirectory(const char *argv0)
{
#ifdef Q_OS_WIN
    wchar_t path[32768] = {};
    constexpr DWORD pathCapacity = DWORD(sizeof(path) / sizeof(path[0]));
    const DWORD length = GetModuleFileNameW(nullptr, path, pathCapacity);
    if (length > 0 && length < pathCapacity) {
        return QFileInfo(QString::fromWCharArray(path, int(length))).absolutePath();
    }
#endif
    return QFileInfo(QString::fromLocal8Bit(argv0 ? argv0 : "")).absolutePath();
}

void prependUnique(QStringList &entries, const QString &entry)
{
    const QString normalized = QDir::toNativeSeparators(QDir::cleanPath(entry));
    for (const QString &existing : entries) {
        if (QDir::toNativeSeparators(QDir::cleanPath(existing))
                .compare(normalized, Qt::CaseInsensitive) == 0) {
            return;
        }
    }
    entries.prepend(normalized);
}

void configureRosRuntime(const char *argv0)
{
    const QString appDir = executableDirectory(argv0);
    const QString localPrefix = QDir(appDir).filePath(QStringLiteral("ros_prefix"));
    const QString defaultRosPrefix = QStringLiteral("C:/opt/ros/foxy/x64");

    QStringList prefixes = QString::fromLocal8Bit(qgetenv("AMENT_PREFIX_PATH"))
                               .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (QDir(defaultRosPrefix).exists()) {
        prependUnique(prefixes, defaultRosPrefix);
    }
    if (QDir(localPrefix).exists()) {
        prependUnique(prefixes, localPrefix);
    }
    if (!prefixes.isEmpty()) {
        qputenv("AMENT_PREFIX_PATH", prefixes.join(QLatin1Char(';')).toLocal8Bit());
    }

    QStringList paths = QString::fromLocal8Bit(qgetenv("PATH"))
                            .split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (QDir(defaultRosPrefix).exists()) {
        prependUnique(paths, QDir(defaultRosPrefix).filePath(QStringLiteral("bin")));
    }
    if (QDir(localPrefix).exists()) {
        prependUnique(paths, QDir(localPrefix).filePath(QStringLiteral("bin")));
    }
    qputenv("PATH", paths.join(QLatin1Char(';')).toLocal8Bit());
}

} // namespace

int main(int argc, char *argv[])
{
    configureRosRuntime(argc > 0 ? argv[0] : nullptr);
    try {
        rclcpp::init(argc, argv);
    } catch (const std::exception &ex) {
        qFatal("rclcpp::init failed: %s", ex.what());
        return 1;
    }
    QApplication a(argc, argv);
    Logger::init("logs");
    LOG("程序启动");

    eApp->init();
    eTheme->setThemeMode(ElaThemeType::Dark);

    MainWindow w;
    w.showMaximized();

    // 数据库引导在后台线程执行，避免登录超时阻塞 GUI 事件循环。
    const auto databaseBootstrapResult = std::make_shared<DatabaseBootstrapResult>();
    QThread *databaseBootstrapThread = QThread::create([databaseBootstrapResult]() {
        for (int attempt = 1; attempt <= kDatabaseBootstrapAttempts; ++attempt) {
            QString attemptError;
            if (DatabaseBootstrap::ensureDatabase(&attemptError)) {
                databaseBootstrapResult->ready = true;
                databaseBootstrapResult->error.clear();
                return;
            }

            databaseBootstrapResult->error = attemptError;
            if (attempt < kDatabaseBootstrapAttempts) {
                LOG("[DB] 数据库引导失败，将进行第 %d/%d 次重试: %s",
                    attempt + 1,
                    kDatabaseBootstrapAttempts,
                    attemptError.toUtf8().constData());
                QThread::sleep(static_cast<unsigned long>(attempt));
            }
        }
    });
    QObject::connect(databaseBootstrapThread, &QThread::finished, &w,
                     [&w, databaseBootstrapResult]() {
        QString dbError = databaseBootstrapResult->error;
        bool databaseReady = databaseBootstrapResult->ready &&
                             Database::instance().open(&dbError);
        if (databaseReady) {
            QString schemaError;
            databaseReady = InspectionRepository::ensureSchema(&schemaError);
            if (!databaseReady) {
                dbError = schemaError;
            }
        }
        if (databaseReady) {
            databaseReady = Database::instance().healthCheck(&dbError);
        }

        if (databaseReady) {
            w.setDatabaseReady(true);
            LOG("数据库连接及健康检查已通过 (SQLite)");
        } else {
            LOG("数据库连接失败: %s", dbError.toUtf8().constData());
            ElaMessageBar::error(ElaMessageBarType::BottomRight,
                                 "数据库",
                                 QStringLiteral("连接失败: %1").arg(dbError),
                                 4000,
                                 &w);
        }
    });
    databaseBootstrapThread->start();

    const int result = a.exec();
    LOG("程序退出，返回码: %d", result);
    if (databaseBootstrapThread->isRunning()) {
        databaseBootstrapThread->wait();
    }
    delete databaseBootstrapThread;
    AuthService::instance().shutdown();
    Database::instance().close();
    if (rclcpp::ok()) {
        rclcpp::shutdown();
    }
    Logger::close();
    return result;
}
