#ifndef ACCOUNTSCHEMA_H
#define ACCOUNTSCHEMA_H

#include <QString>

class QSqlDatabase;

// 账户、会话、审计与应用设置表。DatabaseBootstrap 和 AuthService 都会调用，语句可重复执行。
namespace AccountSchema {

bool ensure(QSqlDatabase &db, QString *errorMessage = nullptr);

} // namespace AccountSchema

#endif // ACCOUNTSCHEMA_H
