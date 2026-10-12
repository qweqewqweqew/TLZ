#ifndef DATABASEBOOTSTRAP_H
#define DATABASEBOOTSTRAP_H

#include <QString>

class DatabaseBootstrap
{
public:
    // 根据 database.ini 准备本地 SQLite 文件并初始化基础表结构。
    // 每次启动可安全调用（幂等）。
    static bool ensureDatabase(QString *errorMessage = nullptr);
};

#endif // DATABASEBOOTSTRAP_H
