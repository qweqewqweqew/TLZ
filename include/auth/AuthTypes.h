#ifndef AUTHTYPES_H
#define AUTHTYPES_H

#include <QString>
#include <QVector>

// 角色逐级包含：管理员拥有技术员的全部权限，技术员拥有操作员的全部权限。
// 数值参与比较，不能改动顺序；落库使用 roleKey() 的文本。
enum class Role {
    None = 0,
    Operator = 1,
    Technician = 2,
    Administrator = 3,
};

// 每个可写项对应一个权限点。新增可写项时在这里加枚举，并在 minimumRole() 里给出级别。
enum class Permission {
    AcknowledgeAlarm,
    EditProcessParams,
    EditDisplaySettings,
    EditCameraConfig,
    EditNodeConfig,
    ManageAccounts,
    ViewAuditLog,
    EditSecurityPolicy,
};

namespace Auth {

QString roleKey(Role role);
Role roleFromKey(const QString &key);
QString roleDisplayName(Role role);
QVector<Role> assignableRoles();

Role minimumRole(Permission permission);
bool roleAllows(Role role, Permission permission);
QString permissionDisplayName(Permission permission);
QVector<Permission> allPermissions();

} // namespace Auth

#endif // AUTHTYPES_H
