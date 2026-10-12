#ifndef TASKFLOWPANEL_H
#define TASKFLOWPANEL_H

#include "bridge/MillingPathVM.h"

#include <QFrame>
#include <QVector>

class QScrollArea;
class QVBoxLayout;

class TaskFlowPanel : public QFrame
{
    Q_OBJECT

public:
    explicit TaskFlowPanel(QWidget *parent = nullptr);

    void setMillingPaths(const QVector<MillingPathVM> &paths);
    void setActivePath(int currentPath);
    // 2D 图点选路径：高亮并滚到对应任务卡（0-based）。
    void selectPath(int pathIndex);
    void clearCommands();

private:
    QWidget *createPathCard(const MillingPathVM &path, int sequence, QWidget *parent);
    void revealExecutedPaths();
    void revealPathsUpTo(int lastIndex);
    void refreshActivePathStyles();
    void scrollToActivePath();
    void scrollToPath(int pathIndex);
    void updateStickyCurrentCard();
    void onScrollValueChanged();
    bool isScrolledAwayFromCurrent() const;

    QScrollArea *m_scrollArea{nullptr};
    QVBoxLayout *m_flowLayout{nullptr};
    QWidget *m_stickyHost{nullptr};
    QVBoxLayout *m_stickyLayout{nullptr};
    QWidget *m_stickyCard{nullptr};
    QVector<QWidget *> m_pathCards;
    QVector<MillingPathVM> m_paths;
    int m_activePathIndex{-1};
    int m_selectedPathIndex{-1};
    int m_stickyPathIndex{-1};
    bool m_browseMode{false};
    bool m_programmaticScroll{false};
};

#endif // TASKFLOWPANEL_H
