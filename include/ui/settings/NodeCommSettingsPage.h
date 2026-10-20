#ifndef NODECOMMSETTINGSPAGE_H
#define NODECOMMSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

class ElaPushButton;
class QFrame;
class QLabel;
class QPlainTextEdit;

class NodeCommSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit NodeCommSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;

public slots:
    // 由 MainWindow 回传：节点进程是否在跑 / 启停脚本是否执行中，用于刷新按钮与状态文字。
    void setNodesRunning(bool running);
    void setNodesBusy(bool busy);

signals:
    void startNodesRequested();
    void stopNodesRequested();

protected:
    void reloadFromSource() override;

private:
    void updateNodeControls();
    void checkNodeReadiness();

    QLabel *m_nodeState{nullptr};
    QLabel *m_readinessSummary{nullptr};
    QLabel *m_cameraReadiness{nullptr};
    QLabel *m_backendReadiness{nullptr};
    QLabel *m_algorithmReadiness{nullptr};
    QLabel *m_cameraDetail{nullptr};
    QLabel *m_backendDetail{nullptr};
    QLabel *m_algorithmDetail{nullptr};
    QFrame *m_cameraCard{nullptr};
    QFrame *m_backendCard{nullptr};
    QFrame *m_algorithmCard{nullptr};
    QLabel *m_readinessDetails{nullptr};
    ElaPushButton *m_startButton{nullptr};
    ElaPushButton *m_stopButton{nullptr};
    ElaPushButton *m_checkButton{nullptr};
    QPlainTextEdit *m_databaseSummary{nullptr};
    bool m_running{false};
    bool m_busy{false};
    bool m_allReady{false};
};

#endif // NODECOMMSETTINGSPAGE_H
