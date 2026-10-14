#include "ui/settings/SettingsPage.h"

#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "auth/AuthService.h"
#include "ui/settings/AboutSettingsPage.h"
#include "ui/settings/AccountSettingsPage.h"
#include "ui/settings/AlarmSettingsPage.h"
#include "ui/settings/DisplaySettingsPage.h"
#include "ui/settings/NodeCommSettingsPage.h"
#include "ui/settings/ProcessSettingsPage.h"
#include "ui/settings/SettingsSubPage.h"
#include "ui/settings/VisionSettingsPage.h"

#include <QAbstractItemView>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

void styleConfirmBox(QMessageBox &box)
{
    box.setStyleSheet(QStringLiteral(
                          "QMessageBox { background-color: %1; }"
                          "QMessageBox QLabel { color: %2; min-width: 320px; font-size: 14px; }")
                          .arg(QLatin1String(Theme::kPanelBg), QLatin1String(Theme::kTextPrimary)));
}

} // namespace

SettingsPage::SettingsPage(Ros2Bridge *bridge,
                           NodeLifecycleMonitor *nodes,
                           EventLogPanel *eventLog,
                           QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("settingsPage"));
    setStyleSheet(QStringLiteral(
                      "QWidget#settingsPage { background: %1; }"
                      "QListWidget#settingsNav {"
                      "  background: %2;"
                      "  color: %3;"
                      "  border: 1px solid %4;"
                      "  border-radius: 8px;"
                      "  outline: none;"
                      "  padding: 6px;"
                      "  font-size: 15px;"
                      "}"
                      "QListWidget#settingsNav::item {"
                      "  color: %3;"
                      "  padding: 10px 12px;"
                      "  border-radius: 6px;"
                      "}"
                      "QListWidget#settingsNav::item:selected {"
                      "  background: %5;"
                      "  color: %6;"
                      "}"
                      "QListWidget#settingsNav::item:hover { background: %7; }")
                      .arg(Theme::kWindowBg,
                           Theme::kPanelBg,
                           Theme::kTextPrimary,
                           Theme::kBorder,
                           Theme::kRunningBg,
                           Theme::kAccent,
                           Theme::kHoverBg));

    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(12);

    auto *navColumn = new QVBoxLayout();
    navColumn->setContentsMargins(0, 0, 0, 0);
    navColumn->setSpacing(8);
    auto *navTitle = makeLabel(QStringLiteral("设置项"), QStringLiteral("panelTitle"));
    navColumn->addWidget(navTitle);
    m_nav = new QListWidget(this);
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->setFixedWidth(196);
    m_nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_nav->setSelectionMode(QAbstractItemView::SingleSelection);
    navColumn->addWidget(m_nav, 1);
    root->addLayout(navColumn);

    auto *contentColumn = new QVBoxLayout();
    contentColumn->setContentsMargins(0, 0, 0, 0);
    contentColumn->setSpacing(8);
    m_pageTitle = makeLabel(QString(), QStringLiteral("panelTitle"));
    contentColumn->addWidget(m_pageTitle);
    m_accessBanner = makeLabel(QString(), QStringLiteral("sectionHint"));
    m_accessBanner->setWordWrap(true);
    contentColumn->addWidget(m_accessBanner);

    // 各子页用页签分区、页签内自行滚动；外层不再包滚动区，否则栈会按最高的子页撑高，短页底部留白。
    m_stack = new QStackedWidget(this);
    contentColumn->addWidget(m_stack, 1);

    auto *actionDivider = new QFrame(this);
    actionDivider->setFixedHeight(1);
    actionDivider->setStyleSheet(QStringLiteral("background:%1;border:none;").arg(Theme::kDivider));
    contentColumn->addWidget(actionDivider);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);
    auto *saveButton = new ElaPushButton(QStringLiteral("保存"), this);
    auto *cancelButton = new ElaPushButton(QStringLiteral("取消"), this);
    auto *defaultButton = new ElaPushButton(QStringLiteral("恢复默认"), this);
    saveButton->setMinimumSize(96, 36);
    cancelButton->setMinimumSize(96, 36);
    defaultButton->setMinimumSize(112, 36);
    applyPrimaryButtonStyle(saveButton);
    applySecondaryButtonStyle(cancelButton);
    applySecondaryButtonStyle(defaultButton);
    m_saveButton = saveButton;
    m_cancelButton = cancelButton;
    m_defaultButton = defaultButton;
    m_status = makeLabel(QString(), QStringLiteral("sectionHint"));
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(36);
    actionRow->addWidget(saveButton);
    actionRow->addWidget(cancelButton);
    actionRow->addWidget(defaultButton);
    actionRow->addWidget(m_status, 1);
    contentColumn->addLayout(actionRow);
    root->addLayout(contentColumn, 1);

    const auto addPage = [this](const QString &title, SettingsSubPage *page) {
        m_nav->addItem(title);
        m_stack->addWidget(page);
        connect(page, &SettingsSubPage::dirtyChanged, this, [this, page](bool) {
            if (page == currentPage()) {
                updateActions();
            }
        });
        connect(page, &SettingsSubPage::editableChanged, this, [this, page]() {
            if (page == currentPage()) {
                updateActions();
            }
        });
        connect(page, &SettingsSubPage::historyRequested, this, &SettingsPage::historyRequested);
        connect(page, &SettingsSubPage::saveRequested, this, [this, page]() {
            if (page == currentPage()) {
                saveCurrent();
            }
        });
    };
    addPage(QStringLiteral("加工与工艺"), new ProcessSettingsPage(bridge, m_stack));
    addPage(QStringLiteral("视觉与相机"), new VisionSettingsPage(m_stack));
    addPage(QStringLiteral("显示与界面"), new DisplaySettingsPage(m_stack));
    addPage(QStringLiteral("节点与通信"), new NodeCommSettingsPage(m_stack));
    addPage(QStringLiteral("报警与诊断"), new AlarmSettingsPage(nodes, eventLog, m_stack));
    addPage(QStringLiteral("权限与账户"), new AccountSettingsPage(m_stack));
    addPage(QStringLiteral("关于"), new AboutSettingsPage(m_stack));

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
    m_stack->setCurrentIndex(row);
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
    if (m_saveButton) {
        m_saveButton->setVisible(hasFields);
        m_saveButton->setEnabled(editable && dirty);
    }
    if (m_cancelButton) {
        m_cancelButton->setVisible(hasFields);
        m_cancelButton->setEnabled(editable && dirty);
    }
    if (m_defaultButton) {
        m_defaultButton->setVisible(hasFields);
        m_defaultButton->setEnabled(editable);
    }
}

void SettingsPage::setStatus(const QString &text, bool problem)
{
    if (!m_status) {
        return;
    }
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
    if (!m_accessBanner) {
        return;
    }
    auto *page = currentPage();
    const std::optional<Permission> permission = page ? page->writePermission() : std::nullopt;
    if (!permission) {
        m_accessBanner->hide();
        return;
    }
    AuthService &auth = AuthService::instance();
    const Role needed = Auth::minimumRole(*permission);
    QString text;
    bool problem = false;
    if (!auth.isAvailable()) {
        text = QStringLiteral("数据库未就绪，账户功能不可用。本页的受控项暂时锁定。");
        problem = true;
    } else if (auth.can(*permission)) {
        text = QStringLiteral("当前身份：%1，可以修改本页（需要「%2」及以上）。")
                   .arg(auth.identityText(), Auth::roleDisplayName(needed));
    } else {
        text = QStringLiteral("只读：修改本页需要「%1」及以上权限，当前身份：%2。点标题栏的账户按钮登录。")
                   .arg(Auth::roleDisplayName(needed), auth.identityText());
        problem = true;
    }
    m_accessBanner->setStyleSheet(QStringLiteral(
                                      "color:%1;font-size:13px;border:none;border-radius:6px;"
                                      "padding:5px 10px;background:%2;")
                                      .arg(problem ? Theme::kStateWarning : Theme::kTextSecondary,
                                           problem ? Theme::kWarningBg : Theme::kPanelBg));
    m_accessBanner->setText(text);
    m_accessBanner->show();
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
