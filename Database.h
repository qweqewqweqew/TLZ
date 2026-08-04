#ifndef DATABASE_H
#define DATABASE_H

#include <QSqlDatabase>
#include <QString>

// 应用级数据库单例：负责打开/关闭到 MzTLZ 的 QODBC 连接
class Database
{
public:
    static Database &instance();

    // 打开启动引导模块选定的 SQL Server 连接（Windows 集成认证）
    // 已打开时直接返回 true
    bool open(QString *errorMessage = nullptr);

    // 关闭并移除连接
    void close();

    bool isOpen() const;

    // 拿到默认连接的 QSqlDatabase 句柄（未打开时返回 invalid 句柄）
    QSqlDatabase handle() const;

    static const char *connectionName();

private:
    Database() = default;
    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;
};

#endif // DATABASE_H
