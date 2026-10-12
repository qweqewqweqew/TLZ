#include "ui/dialogs/BackendParamsDialog.h"

#include "ElaCheckBox.h"
#include "ElaPushButton.h"
#include "app/Logger.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "bridge/Ros2Bridge.h"

#include <QCoreApplication>
#include <QDir>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSettings>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace {

QString settingsFilePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("backend_params.ini"));
}

QString formatOffset(double value)
{
    QString text = QString::number(value, 'f', 3);
    while (text.contains(QLatin1Char('.')) && text.endsWith(QLatin1Char('0'))) {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.'))) {
        text.chop(1);
    }
    if (text.isEmpty() || text == QStringLiteral("-")) {
        return QStringLiteral("0");
    }
    return text;
}

QString taskCardStyle()
{
    return QStringLiteral(R"(
        QFrame#settingsTaskCard {
            background: %1;
            border: 1px solid %2;
            border-radius: 8px;
        }
    )").arg(Theme::kPanelBg, Theme::kBorder);
}

QFrame *makeDivider(QWidget *parent)
{
    auto *line = new QFrame(parent);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(Theme::kDivider));
    return line;
}

QWidget *makeAxisColumn(const QString &title, QLineEdit *edit, QWidget *parent)
{
    auto *col = new QWidget(parent);
    auto *layout = new QVBoxLayout(col);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto *label = makeLabel(title, QStringLiteral("axisTitle"));
    label->setStyleSheet(QStringLiteral(
        "color:%1;font-size:13px;border:none;background:transparent;")
                             .arg(Theme::kTextSecondary));
    layout->addWidget(label);

    auto *valueRow = new QHBoxLayout();
    valueRow->setContentsMargins(0, 0, 0, 0);
    valueRow->setSpacing(8);
    valueRow->addWidget(edit, 1);

    auto *unit = makeLabel(QStringLiteral("mm"), QStringLiteral("sectionHint"));
    unit->setStyleSheet(QStringLiteral(
        "color:%1;font-size:12px;border:none;background:transparent;")
                            .arg(Theme::kTextMuted));
    valueRow->addWidget(unit, 0, Qt::AlignVCenter);
    layout->addLayout(valueRow);
    return col;
}

} // namespace

BackendParamsDialog::BackendParamsDialog(Ros2Bridge *bridge, QWidget *parent)
    : QFrame(parent)
    , m_bridge(bridge)
{
    setObjectName(QStringLiteral("settingsTaskCard"));
    setStyleSheet(taskCardStyle());
    setMinimumWidth(320);
    setMaximumWidth(430);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 14, 16, 16);
    root->setSpacing(14);

    // ---- 标题区 ----
    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(10);

    auto *badge = makeLabel(QStringLiteral("01"), QStringLiteral("taskBadge"));
    badge->setFixedSize(32, 32);
    badge->setAlignment(Qt::AlignCenter);
    badge->setStyleSheet(QStringLiteral(
        "color:%1;font-size:13px;font-weight:700;border:none;border-radius:16px;"
        "background:%2;")
                             .arg(Theme::kAccent, Theme::kRunningBg));
    header->addWidget(badge, 0, Qt::AlignVCenter);

    auto *titleCol = new QVBoxLayout();
    titleCol->setContentsMargins(0, 0, 0, 0);
    titleCol->setSpacing(2);

    auto *title = makeLabel(QStringLiteral("加工参数"), QStringLiteral("taskTitle"));
    title->setStyleSheet(QStringLiteral(
        "color:%1;font-size:16px;font-weight:600;border:none;background:transparent;")
                             .arg(Theme::kTextPrimary));
    titleCol->addWidget(title);

    auto *subtitle = makeLabel(QStringLiteral("坐标补偿 · 路径下发"), QStringLiteral("taskSubtitle"));
    subtitle->setStyleSheet(QStringLiteral(
        "color:%1;font-size:12px;border:none;background:transparent;")
                                .arg(Theme::kTextMuted));
    titleCol->addWidget(subtitle);
    header->addLayout(titleCol, 1);

    auto *kind = makeLabel(QStringLiteral("PLC"), QStringLiteral("taskKind"));
    kind->setAlignment(Qt::AlignCenter);
    kind->setFixedHeight(24);
    kind->setMinimumWidth(52);
    kind->setStyleSheet(QStringLiteral(
        "color:%1;font-size:12px;font-weight:600;border:none;border-radius:4px;"
        "background:%2;padding:0 8px;")
                            .arg(Theme::kRealtime, Theme::kWarningBg));
    header->addWidget(kind, 0, Qt::AlignTop);
    root->addLayout(header);

    root->addWidget(makeDivider(this));

    // ---- XYZ 横向三列 ----
    m_offsetXEdit = makeOffsetEdit(this);
    m_offsetYEdit = makeOffsetEdit(this);
    m_offsetZEdit = makeOffsetEdit(this);

    auto *axisRow = new QHBoxLayout();
    axisRow->setContentsMargins(0, 0, 0, 0);
    axisRow->setSpacing(12);
    axisRow->addWidget(makeAxisColumn(QStringLiteral("X 轴坐标"), m_offsetXEdit, this), 1);
    axisRow->addWidget(makeAxisColumn(QStringLiteral("Y 轴坐标"), m_offsetYEdit, this), 1);
    axisRow->addWidget(makeAxisColumn(QStringLiteral("Z 轴坐标"), m_offsetZEdit, this), 1);
    root->addLayout(axisRow);

    root->addWidget(makeDivider(this));

    // ---- 下发勾选 ----
    m_dispatchPathsCheck = new ElaCheckBox(QStringLiteral("下发路径到 PLC"), this);
    m_dispatchPathsCheck->setChecked(true);
    m_dispatchPathsCheck->setStyleSheet(QStringLiteral(
        "color:%1;font-size:14px;border:none;background:transparent;")
                                            .arg(Theme::kTextPrimary));
    root->addWidget(m_dispatchPathsCheck, 0, Qt::AlignLeft);

    root->addWidget(makeDivider(this));

    // ---- 全宽应用 ----
    auto *applyBtn = new ElaPushButton(QStringLiteral("应用"), this);
    applyBtn->setMinimumHeight(40);
    applyBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyPrimaryButtonStyle(applyBtn);
    root->addWidget(applyBtn);

    connect(applyBtn, &ElaPushButton::clicked, this, &BackendParamsDialog::applyParams);

    loadSettings();
}

QLineEdit *BackendParamsDialog::makeOffsetEdit(QWidget *parent)
{
    auto *edit = new QLineEdit(parent);
    edit->setText(QStringLiteral("0"));
    edit->setFixedHeight(34);
    edit->setMinimumWidth(72);
    edit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    edit->setValidator(new QDoubleValidator(-99999.0, 99999.0, 3, edit));
    edit->setStyleSheet(QStringLiteral(
        "QLineEdit {"
        "  background: %1;"
        "  color: %2;"
        "  border: 1px solid %3;"
        "  border-radius: 6px;"
        "  padding: 0 10px;"
        "  font-size: 14px;"
        "  font-weight: 600;"
        "  selection-background-color: %4;"
        "}"
        "QLineEdit:focus {"
        "  border: 1px solid %5;"
        "}").arg(Theme::kControlBg,
                 Theme::kTextPrimary,
                 Theme::kBorder,
                 Theme::kSelectionBg,
                 Theme::kAccent));
    return edit;
}

void BackendParamsDialog::loadSettings()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("BackendParams"));

    const double x = settings.value(QStringLiteral("OffsetX"), 0.0).toDouble();
    const double y = settings.value(QStringLiteral("OffsetY"), 0.0).toDouble();
    const double z = settings.value(QStringLiteral("OffsetZ"), 0.0).toDouble();
    const bool dispatch = settings.value(QStringLiteral("DispatchPathsToPlc"), true).toBool();
    settings.endGroup();

    if (m_offsetXEdit) {
        m_offsetXEdit->setText(formatOffset(x));
    }
    if (m_offsetYEdit) {
        m_offsetYEdit->setText(formatOffset(y));
    }
    if (m_offsetZEdit) {
        m_offsetZEdit->setText(formatOffset(z));
    }
    if (m_dispatchPathsCheck) {
        m_dispatchPathsCheck->setChecked(dispatch);
    }

    LOG("[设置参数] 已加载本地配置: offset=(%.3f, %.3f, %.3f) mm, dispatch=%s, file=%s",
        x, y, z, dispatch ? "true" : "false",
        settingsFilePath().toUtf8().constData());
}

void BackendParamsDialog::saveSettings() const
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("BackendParams"));
    settings.setValue(QStringLiteral("OffsetX"), offsetX());
    settings.setValue(QStringLiteral("OffsetY"), offsetY());
    settings.setValue(QStringLiteral("OffsetZ"), offsetZ());
    settings.setValue(QStringLiteral("DispatchPathsToPlc"), dispatchPathsToPlc());
    settings.endGroup();
    settings.sync();

    LOG("[设置参数] 已保存本地配置: offset=(%.3f, %.3f, %.3f) mm, dispatch=%s, file=%s",
        offsetX(), offsetY(), offsetZ(),
        dispatchPathsToPlc() ? "true" : "false",
        settingsFilePath().toUtf8().constData());
}

double BackendParamsDialog::offsetX() const
{
    return m_offsetXEdit ? m_offsetXEdit->text().toDouble() : 0.0;
}

double BackendParamsDialog::offsetY() const
{
    return m_offsetYEdit ? m_offsetYEdit->text().toDouble() : 0.0;
}

double BackendParamsDialog::offsetZ() const
{
    return m_offsetZEdit ? m_offsetZEdit->text().toDouble() : 0.0;
}

bool BackendParamsDialog::dispatchPathsToPlc() const
{
    return m_dispatchPathsCheck && m_dispatchPathsCheck->isChecked();
}

void BackendParamsDialog::applyParams()
{
    if (!m_bridge) {
        LOG("[设置参数] 应用失败: Ros2Bridge 为空");
        return;
    }

    const double x = offsetX();
    const double y = offsetY();
    const double z = offsetZ();
    const bool dispatch = dispatchPathsToPlc();

    LOG("[设置参数] 用户点击应用: offset=(%.3f, %.3f, %.3f) mm, dispatch_paths_to_plc=%s",
        x, y, z, dispatch ? "true" : "false");

    m_bridge->publishCoordinateOffset(x, y, z);
    m_bridge->publishDispatchPathsToPlc(dispatch);
    saveSettings();
}
