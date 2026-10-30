#include "ui/settings/SettingsPage.h"

#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "auth/AuthService.h"
#include "ui/settings/AccountSettingsPage.h"
#include "ui/settings/CalibrationPlaceholderPage.h"
#include "ui/settings/NodeCommSettingsPage.h"
#include "ui/settings/ProcessCraftPlaceholderPage.h"
#include "ui/settings/ProcessSettingsPage.h"
#include "ui/settings/SettingsSubPage.h"
#include "ui/settings/VisionSettingsPage.h"
#include "vision/PointCloudFrame.h"

#include <QAbstractItemView>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListView>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

void styleConfirmBox(QMessageBox &box)
{
    box.setStyleSheet(QStringLiteral(
                          "QMessageBox { background-color: %1; }"
                          "QMessageBox QLabel { color: %2; min-width: 320px; font-size: 14px; }")
                          .arg(QLatin1String(Theme::kPanelBg), QLatin1String(Theme::kTextPrimary)));
}

// 顶部二级导航：矮卡片 + 左图标右文字。卡片本身由 setItemWidget 的自定义 QWidget 绘制，
// 委托层不再画背景，避免选中/悬停样式被覆盖后出现重影。
QString settingsPageStyleSheet()
{
    return QStringLiteral(
               "QWidget#settingsPage { background: %1; }"
               "QListWidget#settingsNav {"
               "  background: transparent; color: %3; border: none;"
               "  outline: none; padding: 0; font-size: 14px;"
               "}"
               "QListWidget#settingsNav::item {"
               "  background: transparent; border: none; padding: 0;"
               "}"
               "QWidget#navCard {"
               "  background: %2; border: 1px solid %4; border-radius: 6px;"
               "}"
               "QWidget#navCard:hover {"
               "  background: %7; border-color: %6;"
               "}"
               "QWidget#navCard[selected=\"true\"] {"
               "  background: %5; border-color: %6;"
               "}")
        .arg(Theme::kCanvasBg,
             Theme::kPanelBg,
             Theme::kTextPrimary,
             Theme::kBorder,
             Theme::kSelectionBg,
             Theme::kFocusRing,
             Theme::kHoverBg);
}

} // namespace

SettingsPage::SettingsPage(Ros2Bridge *bridge,
                           NodeLifecycleMonitor *nodes,
                           EventLogPanel *eventLog,
                           QWidget *parent)
    : QWidget(parent)
{
    Q_UNUSED(bridge);
    Q_UNUSED(nodes);
    Q_UNUSED(eventLog);
    setObjectName(QStringLiteral("settingsPage"));
    setStyleSheet(settingsPageStyleSheet());
    Theme::watch(this, [this]() {
        setStyleSheet(settingsPageStyleSheet());
        updateNavIcons();
        updateAccessBanner();
        if (m_status && !m_status->text().isEmpty()) {
            setStatus(m_status->text(), m_status->property("problem").toBool());
        }
    });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(8);

    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->setFixedHeight(54);
    m_nav->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_nav->setIconSize(QSize(20, 20));
    m_nav->setUniformItemSizes(true);
    m_nav->setSpacing(6);
    m_nav->setViewMode(QListView::IconMode);
    m_nav->setLayoutMode(QListView::SinglePass);
    m_nav->setFlow(QListView::LeftToRight);
    m_nav->setWrapping(false);
    m_nav->setResizeMode(QListView::Adjust);
    m_nav->setMovement(QListView::Static);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_nav->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setSelectionMode(QAbstractItemView::SingleSelection);
    root->addWidget(m_nav);

    auto *contentColumn = new QVBoxLayout();
    contentColumn->setContentsMargins(0, 0, 0, 0);
    contentColumn->setSpacing(12);
    m_pageTitle = makeLabel(QString(), QStringLiteral("panelTitle"));
    m_pageTitle->setVisible(false);
    contentColumn->addWidget(m_pageTitle);
    m_accessBanner = makeLabel(QString(), QStringLiteral("sectionHint"));
    m_accessBanner->setWordWrap(true);
    m_accessBanner->hide();

    // 各子页用页签分区、页签内自行滚动；外层不再包滚动区，否则栈会按最高的子页撑高，短页底部留白。
    m_stack = new QStackedWidget(this);
    contentColumn->addWidget(m_stack, 1);

    auto *actionDivider = new QFrame(this);
    actionDivider->setObjectName(QStringLiteral("settingsActionDivider"));
    actionDivider->setFixedHeight(1);
    actionDivider->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(Theme::kDivider));
    contentColumn->addWidget(actionDivider);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    auto *saveButton = new ElaPushButton(QStringLiteral("保存"), this);
    auto *cancelButton = new ElaPushButton(QStringLiteral("取消"), this);
    auto *defaultButton = new ElaPushButton(QStringLiteral("恢复默认"), this);
    saveButton->setMinimumSize(84, 30);
    cancelButton->setMinimumSize(84, 30);
    defaultButton->setMinimumSize(100, 30);
    applyPrimaryButtonStyle(saveButton);
    applySecondaryButtonStyle(cancelButton);
    applySecondaryButtonStyle(defaultButton);
    m_saveButton = saveButton;
    m_cancelButton = cancelButton;
    m_defaultButton = defaultButton;
    m_status = makeLabel(QString(), QStringLiteral("sectionHint"));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(28);
    actionRow->addWidget(saveButton);
    actionRow->addWidget(cancelButton);
    actionRow->addWidget(defaultButton);
    actionRow->addWidget(m_status, 1);
    contentColumn->addLayout(actionRow);
    root->addLayout(contentColumn, 1);

    const auto addPage = [this](const QString &title, const QString &iconPath, QWidget *page) {
        auto *item = new QListWidgetItem(title, m_nav);
        item->setData(Qt::UserRole, iconPath);
        item->setTextAlignment(Qt::AlignCenter);

        // 左图标 + 右文字，整体水平居中；卡片承载背景 / 悬停 / 选中样式。
        auto *card = new QWidget(m_nav);
        card->setObjectName(QStringLiteral("navCard"));
        card->setAttribute(Qt::WA_StyledBackground, true);
        card->setAttribute(Qt::WA_Hover, true);
        card->setProperty("selected", false);
        auto *cardLayout = new QHBoxLayout(card);
        cardLayout->setContentsMargins(10, 0, 10, 0);
        cardLayout->setSpacing(8);
        cardLayout->setAlignment(Qt::AlignCenter);
        auto *iconLabel = new QLabel(card);
        iconLabel->setObjectName(QStringLiteral("navIcon"));
        iconLabel->setFixedSize(20, 20);
        iconLabel->setScaledContents(true);
        iconLabel->setPixmap(tintedIcon(iconPath, QColor(Theme::kTextPrimary), 20,
                                        m_nav->devicePixelRatioF()));
        auto *textLabel = new QLabel(title, card);
        textLabel->setObjectName(QStringLiteral("navText"));
        textLabel->setStyleSheet(QStringLiteral(
            "color:%1; font-size:14px; font-weight:700; border:none; background:transparent;")
            .arg(Theme::kTextPrimary));
        cardLayout->addWidget(iconLabel, 0, Qt::AlignVCenter);
        cardLayout->addWidget(textLabel, 0, Qt::AlignVCenter);
        m_nav->setItemWidget(item, card);

        m_stack->addWidget(page);
        if (auto *subPage = qobject_cast<SettingsSubPage *>(page)) {
            connect(subPage, &SettingsSubPage::dirtyChanged, this, [this, subPage](bool) {
                if (subPage == currentPage()) {
                    updateActions();
                }
            });
            connect(subPage, &SettingsSubPage::editableChanged, this, [this, subPage]() {
                if (subPage == currentPage()) {
                    updateActions();
                }
            });
            connect(subPage, &SettingsSubPage::historyRequested, this, &SettingsPage::historyRequested);
            connect(subPage, &SettingsSubPage::saveRequested, this, [this, subPage]() {
                if (subPage == currentPage()) {
                    saveCurrent();
                }
            });
        }
    };
    m_nodeCommPage = new NodeCommSettingsPage(m_stack);
    addPage(QStringLiteral("设备与通信"), QStringLiteral(":/img/nav_settings.svg"), m_nodeCommPage);
    m_calibrationPage = new CalibrationPlaceholderPage(m_stack);
    addPage(QStringLiteral("检测与标定"), QStringLiteral(":/img/nav_detect.svg"),
            m_calibrationPage);
    addPage(QStringLiteral("修磨工艺"), QStringLiteral(":/img/nav_process.svg"),
            new ProcessCraftPlaceholderPage(m_stack));
    addPage(QStringLiteral("安全与权限"), QStringLiteral(":/img/nav_security.svg"),
            new AccountSettingsPage(m_stack));
    connect(m_nodeCommPage, &NodeCommSettingsPage::startNodesRequested,
            this, &SettingsPage::startNodesRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::stopNodesRequested,
            this, &SettingsPage::stopNodesRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::hikCameraConnectRequested,
            this, &SettingsPage::hikCameraConnectRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::hikCameraDisconnectRequested,
            this, &SettingsPage::hikCameraDisconnectRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::hikCameraRefreshParametersRequested,
            this, &SettingsPage::hikCameraRefreshParametersRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::hikCameraParametersUpdateRequested,
            this, &SettingsPage::hikCameraParametersUpdateRequested);
    connect(m_nodeCommPage, &NodeCommSettingsPage::sickCameraParametersUpdateRequested,
            this, &SettingsPage::sickCameraParametersUpdateRequested);

    connect(m_nav, &QListWidget::currentRowChanged, this, [this](int row) {
        if (m_updatingNav || row < 0 || row == m_currentRow) {
            return;
        }
        if (!confirmLeave()) {
            m_updatingNav = true;
            m_nav->setCurrentRow(m_currentRow);
            m_updatingNav = false;
            return;
        }
        m_currentRow = row;
        showPage(row);
        updateNavSelection();
    });
    connect(saveButton, &ElaPushButton::clicked, this, [this]() { saveCurrent(); });
    connect(cancelButton, &ElaPushButton::clicked, this, [this]() {
        if (auto *page = currentPage()) {
            page->revert();
            setStatus(page->bindingSummary(), false);
            updateActions();
        }
    });
    connect(defaultButton, &ElaPushButton::clicked, this, [this]() {
        if (auto *page = currentPage()) {
            if (!canWrite(page)) {
                return;
            }
            page->restoreDefaults();
            setStatus(QStringLiteral("已填入该页默认值，尚未写入文件。确认后点「保存」。"), false);
            updateActions();
        }
    });

    connect(&AuthService::instance(), &AuthService::sessionChanged, this, &SettingsPage::refreshAccess);

    m_updatingNav = true;
    m_nav->setCurrentRow(0);
    m_updatingNav = false;
    m_currentRow = 0;
    refreshAccess();
    showPage(0);
    updateNavCardSizes();
    updateNavSelection();
}

void SettingsPage::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    QTimer::singleShot(0, this, &SettingsPage::updateNavCardSizes);
}

void SettingsPage::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    QTimer::singleShot(0, this, &SettingsPage::updateNavCardSizes);
}

void SettingsPage::updateNavCardSizes()
{
    if (!m_nav || m_nav->count() <= 0) {
        return;
    }
    // The list viewport is the actual drawable width after scrollbars and layout margins.
    const int available = qMax(320, m_nav->viewport()->width());
    const int cardWidth = qMax(120, (available - m_nav->spacing() * (m_nav->count() - 1)) / m_nav->count());
    m_nav->setGridSize(QSize(cardWidth, 46));
    for (int i = 0; i < m_nav->count(); ++i) {
        if (auto *item = m_nav->item(i)) {
            item->setSizeHint(QSize(cardWidth, 46));
        }
    }
    m_nav->doItemsLayout();
}

void SettingsPage::updateNavIcons()
{
    if (!m_nav) {
        return;
    }
    const QColor color(Theme::kTextPrimary);
    const qreal dpr = m_nav->devicePixelRatioF();
    for (int i = 0; i < m_nav->count(); ++i) {
        auto *item = m_nav->item(i);
        if (!item) {
            continue;
        }
        auto *card = m_nav->itemWidget(item);
        if (!card) {
            continue;
        }
        const QString iconPath = item->data(Qt::UserRole).toString();
        if (auto *icon = card->findChild<QLabel *>(QStringLiteral("navIcon"))) {
            if (!iconPath.isEmpty()) {
                icon->setPixmap(tintedIcon(iconPath, color, 20, dpr));
            }
        }
        if (auto *text = card->findChild<QLabel *>(QStringLiteral("navText"))) {
            text->setStyleSheet(QStringLiteral(
                "color:%1; font-size:14px; font-weight:700; border:none; background:transparent;")
                .arg(color.name()));
        }
    }
}

void SettingsPage::updateNavSelection()
{
    if (!m_nav) {
        return;
    }
    const int current = m_nav->currentRow();
    for (int i = 0; i < m_nav->count(); ++i) {
        auto *item = m_nav->item(i);
        auto *card = item ? m_nav->itemWidget(item) : nullptr;
        if (!card) {
            continue;
        }
        const bool selected = (i == current);
        if (card->property("selected").toBool() == selected) {
            continue;
        }
        card->setProperty("selected", selected);
        card->style()->unpolish(card);
        card->style()->polish(card);
        card->update();
    }
}

void SettingsPage::setNodesRunning(bool running)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setNodesRunning(running);
    }
}

void SettingsPage::setNodesBusy(bool busy)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setNodesBusy(busy);
    }
}

void SettingsPage::setCameraNodeStatus(bool connected, const QString &detail)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setCameraNodeStatus(connected, detail);
    }
    if (m_visionPage) {
        m_visionPage->setCameraNodeStatus(connected, detail);
    }
}

void SettingsPage::setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setCameraTopicDiagnostics(bridgeRunning, publisherCount);
    }
    if (m_visionPage) {
        m_visionPage->setCameraTopicDiagnostics(bridgeRunning, publisherCount);
    }
}

void SettingsPage::setHikCameraStatus(bool connected, const QString &detail)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setHikCameraStatus(connected, detail);
    }
    if (m_visionPage) {
        m_visionPage->setHikCameraStatus(connected, detail);
    }
}

void SettingsPage::setHikCameraParameters(const HikCameraParameters &parameters)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setHikCameraParameters(parameters);
    }
}

void SettingsPage::setSickCameraParameters(const SickCameraParametersVM &parameters)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setSickCameraParameters(parameters);
    }
}

void SettingsPage::setSickCameraParameterResult(bool success,
                                                const QString &message,
                                                const SickCameraParametersVM &parameters)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setSickCameraParameterResult(success, message, parameters);
    }
}

void SettingsPage::setScanFrame(const PointCloudFrame &frame)
{
    if (m_calibrationPage) {
        m_calibrationPage->setScanFrame(frame);
    }
}

void SettingsPage::setPlcDeviceStatus(const QString &status, const QString &detail)
{
    if (m_nodeCommPage) {
        m_nodeCommPage->setPlcDeviceStatus(status, detail);
    }
}

bool SettingsPage::confirmLeave()
{
    auto *page = currentPage();
    if (!page || !page->isDirty()) {
        return true;
    }

    QMessageBox box(QMessageBox::Question,
                    QStringLiteral("设置未保存"),
                    QStringLiteral("当前子页有未保存的修改。"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                    this);
    box.setButtonText(QMessageBox::Save, QStringLiteral("保存"));
    box.setButtonText(QMessageBox::Discard, QStringLiteral("放弃修改"));
    box.setButtonText(QMessageBox::Cancel, QStringLiteral("留在本页"));
    box.setDefaultButton(QMessageBox::Cancel);
    box.setEscapeButton(QMessageBox::Cancel);
    styleConfirmBox(box);

    const int choice = box.exec();
    if (choice == QMessageBox::Save) {
        return saveCurrent();
    }
    if (choice == QMessageBox::Discard) {
        page->revert();
        updateActions();
        return true;
    }
    return false;
}

void SettingsPage::showPage(int row)
{
    if (!m_stack || row < 0 || row >= m_stack->count()) {
        return;
    }
    fadeToStackedWidgetPage(m_stack, m_stack->widget(row));
    if (m_pageTitle && m_nav && m_nav->item(row)) {
        m_pageTitle->setText(m_nav->item(row)->text());
    }
    if (auto *page = currentPage()) {
        setStatus(page->bindingSummary(), false);
    }
    updateAccessBanner();
    updateActions();
}

void SettingsPage::updateActions()
{
    auto *page = currentPage();
    const bool hasFields = page && page->isEditable();
    const bool editable = hasFields && canWrite(page);
    const bool dirty = page && page->isDirty();
    // 设备与通信页在相机卡片内提供应用/恢复默认，避免底部出现双层操作栏。
    const bool showGlobalActions = hasFields && page != m_nodeCommPage;
    if (auto *divider = findChild<QFrame *>(QStringLiteral("settingsActionDivider"))) {
        divider->setVisible(showGlobalActions);
    }
    if (m_status) {
        m_status->setVisible(showGlobalActions);
    }
    if (m_saveButton) {
        m_saveButton->setVisible(showGlobalActions);
        m_saveButton->setEnabled(editable && dirty);
    }
    if (m_cancelButton) {
        m_cancelButton->setVisible(showGlobalActions);
        m_cancelButton->setEnabled(editable && dirty);
    }
    if (m_defaultButton) {
        m_defaultButton->setVisible(showGlobalActions);
        m_defaultButton->setEnabled(editable);
    }
}

void SettingsPage::setStatus(const QString &text, bool problem)
{
    if (!m_status) {
        return;
    }
    m_status->setProperty("problem", problem);
    m_status->setStyleSheet(QStringLiteral(
                                "color:%1;font-size:12px;border:none;background:transparent;")
                                .arg(problem ? Theme::kStateError : Theme::kTextSecondary));
    m_status->setText(text);
}

bool SettingsPage::saveCurrent()
{
    auto *page = currentPage();
    if (!page || !page->isEditable()) {
        setStatus(QStringLiteral("本页没有可写项，未写入任何配置。"), false);
        return true;
    }

    AuthService &auth = AuthService::instance();
    const QString title = pageTitle(m_currentRow);
    if (!canWrite(page)) {
        const Role needed = Auth::minimumRole(*page->writePermission());
        const QString message = QStringLiteral("保存被拒绝：本页需要「%1」及以上权限，当前身份：%2。")
                                    .arg(Auth::roleDisplayName(needed), auth.identityText());
        auth.audit(QStringLiteral("settings.save"), title, QStringLiteral("权限不足"), false);
        setStatus(message, true);
        ElaMessageBar::error(ElaMessageBarType::BottomRight,
                             QStringLiteral("设置未保存"),
                             message,
                             3600,
                             this);
        emit logRequested(QStringLiteral("WARN"), message);
        return false;
    }

    const QString summary = page->pendingChangeSummary();
    const SettingsSaveOutcome outcome = page->save();
    const bool rejected = outcome.status == SettingsSaveStatus::Rejected;
    const bool warning = outcome.status == SettingsSaveStatus::SavedWithWarning;
    QString detail = summary.isEmpty() ? QStringLiteral("（无字段摘要）") : summary;
    if (rejected || warning) {
        detail += QStringLiteral("；结果：%1").arg(outcome.message);
    }
    auth.audit(QStringLiteral("settings.save"), title, detail, !rejected);
    setStatus(outcome.message, rejected || warning);
    if (rejected) {
        ElaMessageBar::error(ElaMessageBarType::BottomRight,
                             QStringLiteral("设置未保存"),
                             outcome.message,
                             3600,
                             this);
        emit logRequested(QStringLiteral("ERROR"), outcome.message);
        return false;
    }
    if (warning) {
        ElaMessageBar::warning(ElaMessageBarType::BottomRight,
                               QStringLiteral("设置已部分保存"),
                               outcome.message,
                               3600,
                               this);
        emit logRequested(QStringLiteral("WARN"), outcome.message);
    } else {
        ElaMessageBar::success(ElaMessageBarType::BottomRight,
                               QStringLiteral("设置已保存"),
                               outcome.message,
                               2400,
                               this);
        emit logRequested(QStringLiteral("INFO"), outcome.message);
    }
    updateActions();
    return true;
}

bool SettingsPage::canWrite(const SettingsSubPage *page) const
{
    if (!page) {
        return false;
    }
    const std::optional<Permission> permission = page->writePermission();
    return !permission || AuthService::instance().can(*permission);
}

void SettingsPage::refreshAccess()
{
    if (!m_stack) {
        return;
    }
    for (int i = 0; i < m_stack->count(); ++i) {
        auto *page = qobject_cast<SettingsSubPage *>(m_stack->widget(i));
        if (!page) {
            continue;
        }
        const bool allowed = canWrite(page);
        // 会话结束（超时、退出、切换账户）时丢弃未保存的修改，避免下一位用户直接保存。
        if (!allowed && page->isDirty()) {
            page->revert();
        }
        page->setWriteAllowed(allowed);
    }
    updateAccessBanner();
    updateActions();
}

void SettingsPage::updateAccessBanner()
{
    // 权限仍由页面控件和保存动作校验，这个状态提示不再占用设置页空间。
    if (m_accessBanner) {
        m_accessBanner->hide();
    }
}

QString SettingsPage::pageTitle(int row) const
{
    if (m_nav && m_nav->item(row)) {
        return m_nav->item(row)->text();
    }
    return QString();
}

SettingsSubPage *SettingsPage::currentPage() const
{
    return m_stack ? qobject_cast<SettingsSubPage *>(m_stack->currentWidget()) : nullptr;
}
