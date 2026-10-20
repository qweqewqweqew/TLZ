#include "ui/panels/ParticleDetailPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "vision/HeightMap3DFrame.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {


class ParticlePreview : public QWidget
{
public:
    explicit ParticlePreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(280, 300);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_cloud = new HeightMap3DFrame(this);
        m_cloud->setObjectName(QStringLiteral("heightMap3dArea"));
        m_cloud->setToolTip(QStringLiteral("左键拖动旋转，滚轮缩放"));
        layout->addWidget(m_cloud);
    }

    HeightMap3DFrame *cloud() const { return m_cloud; }

    QSize sizeHint() const override { return QSize(320, 360); }

    void setFrame(const PointCloudFrame &frame)
    {
        if (m_cloud) {
            m_cloud->setPointCloudFrame(frame);
        }
    }

private:
    HeightMap3DFrame *m_cloud{nullptr};
};

} // namespace

ParticleDetailPanel::ParticleDetailPanel(QWidget *parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("particleStage"));
    setFrameShape(QFrame::NoFrame);
    setStyleSheet(QStringLiteral("QFrame#particleStage { background: transparent; border: none; }"));

    auto *body = new QHBoxLayout(this);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(18);

    auto *cloudColumn = new QWidget(this);
    cloudColumn->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    auto *cloudLayout = new QVBoxLayout(cloudColumn);
    cloudLayout->setContentsMargins(0, 0, 0, 0);
    cloudLayout->setSpacing(8);
    auto *cloudTitle = new QLabel(QStringLiteral("三维点云"), cloudColumn);
    Theme::bindStyle(cloudTitle, []() {
        return QStringLiteral("color:%1;font-size:16px;font-weight:600;background:transparent;border:none;")
            .arg(Theme::kTitle);
    });
    cloudLayout->addWidget(cloudTitle);
    auto *preview = new ParticlePreview(cloudColumn);
    m_preview = preview;
    m_heightMap = preview->cloud();
    cloudLayout->addWidget(preview, 1);
    body->addWidget(cloudColumn, 1);

    m_sideHost = new QWidget(this);
    m_sideHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_sideLayout = new QVBoxLayout(m_sideHost);
    m_sideLayout->setContentsMargins(0, 0, 0, 0);
    m_sideLayout->setSpacing(8);
    auto *dataTitle = new QLabel(QStringLiteral("实时数据"), m_sideHost);
    Theme::bindStyle(dataTitle, []() {
        return QStringLiteral("color:%1;font-size:16px;font-weight:600;background:transparent;border:none;")
            .arg(Theme::kTitle);
    });
    m_sideLayout->addWidget(dataTitle, 0);
    body->addWidget(m_sideHost, 1);

    clear();
    Theme::watch(this, [this]() { refresh(); });
}

void ParticleDetailPanel::setPointCloudFrame(const PointCloudFrame &frame)
{
    m_frame = frame;
    if (m_preview) {
        static_cast<ParticlePreview *>(m_preview)->setFrame(frame);
    } else if (m_heightMap) {
        m_heightMap->setPointCloudFrame(frame);
    }
    refresh();
}

void ParticleDetailPanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    m_selected = -1;
    m_completed = 0;
    m_current = 0;
    refresh();
}

void ParticleDetailPanel::setTaskInfo(int maxParticleHeight, const QString &grade)
{
    m_maxParticleHeight = maxParticleHeight;
    m_grade = grade;
    refresh();
}

void ParticleDetailPanel::selectPath(int pathIndex)
{
    m_selected = (pathIndex >= 0 && pathIndex < m_paths.size()) ? pathIndex : -1;
    refresh();
}

void ParticleDetailPanel::setPathProgress(int completedPath, int currentPath)
{
    if (m_completed == completedPath && m_current == currentPath) {
        return;
    }
    m_completed = completedPath;
    m_current = currentPath;
    refresh();
}

void ParticleDetailPanel::setTaskProgress(float progressPercent)
{
    Q_UNUSED(progressPercent);
}

void ParticleDetailPanel::setSideContent(QWidget *content)
{
    if (!content || !m_sideLayout || !m_sideHost) {
        return;
    }
    content->setParent(m_sideHost);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_sideLayout->addWidget(content, 1);
}

void ParticleDetailPanel::clear()
{
    m_frame = PointCloudFrame();
    m_paths.clear();
    m_selected = -1;
    m_completed = 0;
    m_current = 0;
    m_maxParticleHeight = -1;
    m_grade.clear();
    if (m_preview) {
        static_cast<ParticlePreview *>(m_preview)->setFrame(PointCloudFrame());
    } else if (m_heightMap) {
        m_heightMap->clearPointCloud();
    }
    setTaskProgress(0.0f);
    refresh();
}

void ParticleDetailPanel::refresh()
{
}
