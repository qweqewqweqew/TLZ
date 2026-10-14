#include "auth/AuthTypes.h"

namespace Auth {

QString roleKey(Role role)
{
    switch (role) {
    case Role::Operator:
        return QStringLiteral("operator");
    case Role::Technician:
        return QStringLiteral("technician");
    case Role::Administrator:
        return QStringLiteral("administrator");
    case Role::None:
        break;
    }
    return QString();
}

Role roleFromKey(const QString &key)
{
    if (key == QLatin1String("operator")) {
        return Role::Operator;
    }
    if (key == QLatin1String("technician")) {
        return Role::Technician;
    }
    if (key == QLatin1String("administrator")) {
        return Role::Administrator;
    }
    return Role::None;
}

QString roleDisplayName(Role role)
{
    switch (role) {
    case Role::Operator:
        return QStringLiteral("操作员");
    case Role::Technician:
        return QStringLiteral("技术员");
    case Role::Administrator:
        return QStringLiteral("管理员");
    case Role::None:
        break;
    }
    return QStringLiteral("未登录");
}

QVector<Role> assignableRoles()
{
    return {Role::Operator, Role::Technician, Role::Administrator};
}

Role minimumRole(Permission permission)
{
    switch (permission) {
    case Permission::AcknowledgeAlarm:
        return Role::Operator;
    case Permission::EditProcessParams:
    case Permission::EditDisplaySettings:
        return Role::Technician;
    case Permission::EditCameraConfig:
    case Permission::EditNodeConfig:
    case Permission::ManageAccounts:
    case Permission::ViewAuditLog:
    case Permission::EditSecurityPolicy:
        return Role::Administrator;
    }
    return Role::Administrator;
}

bool roleAllows(Role role, Permission permission)
{
    if (role == Role::None) {
        return false;
    }
    return static_cast<int>(role) >= static_cast<int>(minimumRole(permission));
}

QString permissionDisplayName(Permission permission)
{
    switch (permission) {
    case Permission::AcknowledgeAlarm:
        return QStringLiteral("确认报警");
    case Permission::EditProcessParams:
        return QStringLiteral("修改加工偏移、路径下发，执行示教取坐标");
    case Permission::EditDisplaySettings:
        return QStringLiteral("修改显示与界面设置");
    case Permission::EditCameraConfig:
        return QStringLiteral("修改海康相机连接参数");
    case Permission::EditNodeConfig:
        return QStringLiteral("修改相机节点配置");
    case Permission::ManageAccounts:
        return QStringLiteral("新建账户、重置密码、修改角色、停用与解锁");
    case Permission::ViewAuditLog:
        return QStringLiteral("查看操作审计");
    case Permission::EditSecurityPolicy:
        return QStringLiteral("修改会话超时");
    }
    return QString();
}

QVector<Permission> allPermissions()
{
    return {
        Permission::AcknowledgeAlarm,
        Permission::EditProcessParams,
        Permission::EditDisplaySettings,
        Permission::EditCameraConfig,
        Permission::EditNodeConfig,
        Permission::ManageAccounts,
        Permission::ViewAuditLog,
        Permission::EditSecurityPolicy,
    };
}

} // namespace Auth
