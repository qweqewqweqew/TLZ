#include "mainwindow.h"
#include "Database.h"
#include "InspectionRepository.h"
#include "Logger.h"

#include "ElaApplication.h"
#include "ElaMessageBar.h"
#include "ElaTheme.h"
#include <QDebug>
#include <rclcpp/rclcpp.hpp>

#include <QApplication>
#include <QTimer>

#include <cstdio>

int main(int argc, char *argv[])
{
    FILE *f = fopen("D:/crash_debug.txt", "w");
    if (f) { fprintf(f, "main() entered\n"); fflush(f); }
    try {
        if (f) { fprintf(f, "before rclcpp::init\n"); fflush(f); }
        rclcpp::init(argc, argv);
        if (f) { fprintf(f, "after rclcpp::init\n"); fflush(f); }
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

    // 打开数据库连接（失败不阻断程序，只记录日志并在界面上给个提示）
    {
        QString dbError;
        if (Database::instance().open(&dbError)) {
            LOG("数据库连接已建立 (MzTLZ)");
            // 启动时显式执行一次幂等结构迁移（BackendTaskId / InspectionFrame）
            {
                QString schemaErr;
                if (!InspectionRepository::ensureSchema(&schemaErr)) {
                    LOG("[DB] 结构迁移失败: %s", schemaErr.toUtf8().constData());
                }
            }
            // 首次启动时如果历史记录为空，插入几条模拟检测记录用于演示
            QTimer::singleShot(0, &w, []() {
                QString err;
                const auto existing = InspectionRepository::loadRecent(1, &err);
                if (!err.isEmpty()) {
                    LOG("[Seed] 检查历史记录失败: %s", err.toUtf8().constData());
                    return;
                }
                if (!existing.isEmpty()) {
                    return;
                }
                QString mockErr;
                const int n = InspectionRepository::insertMockRecords(5, &mockErr);
                if (!mockErr.isEmpty()) {
                    LOG("[Seed] 模拟数据种子失败: %s", mockErr.toUtf8().constData());
                } else {
                    LOG("[Seed] 已插入 %d 条模拟历史记录", n);
                }
            });
        } else {
            LOG("数据库连接失败: %s", dbError.toUtf8().constData());
            QTimer::singleShot(500, &w, [&w, dbError]() {
                ElaMessageBar::error(ElaMessageBarType::BottomRight,
                                     "数据库",
                                     QStringLiteral("连接失败: %1").arg(dbError),
                                     4000,
                                     &w);
            });
        }
    }

    const int result = a.exec();
    LOG("程序退出，返回码: %d", result);
    Database::instance().close();
    if (rclcpp::ok()) {
        rclcpp::shutdown();
    }
    Logger::close();
    return result;
}
