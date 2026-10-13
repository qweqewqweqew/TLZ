#include "ui/settings/SettingsPage.h"

#include "ElaMessageBar.h"
#include "ElaPushButton.h"
#include "app/Theme.h"
#include "app/UiHelpers.h"
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
#include <QScrollArea>
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
                      "QListWidget#settingsNav::item:hover { background: %7; }"
                      "QScrollArea#settingsScroll { background: transparent; border: none; }")
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

    m_stack = new QStackedWidget(this);
    auto *scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("settingsScroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(m_stack);
    contentColumn->addWidget(scroll, 1);

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
        connect(page, &SettingsSubPage::historyRequested, this, &SettingsPage::historyRequested);
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
            page->restoreDefaults();
            setStatus(QStringLiteral("已填入该页默认值，尚未写入文件。确认后点「保存」。"), false);
            updateActions();
        }
    });

    m_updatingNav = true;
    m_nav->setCurrentRow(0);
    m_updatingNav = false;
    m_currentRow = 0;
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
    updateActions();
}

void SettingsPage::updateActions()
{
    auto *page = currentPage();
    const bool editable = page && page->isEditable();
    const bool dirty = page && page->isDirty();
    if (m_saveButton) {
        m_saveButton->setEnabled(editable && dirty);
    }
    if (m_cancelButton) {
        m_cancelButton->setEnabled(editable && dirty);
    }
    if (m_defaultButton) {
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

    const SettingsSaveOutcome outcome = page->save();
    const bool rejected = outcome.status == SettingsSaveStatus::Rejected;
    const bool warning = outcome.status == SettingsSaveStatus::SavedWithWarning;
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

SettingsSubPage *SettingsPage::currentPage() const
{
    return m_stack ? qobject_cast<SettingsSubPage *>(m_stack->currentWidget()) : nullptr;
}
