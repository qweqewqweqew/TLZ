#include "ui/panels/ParticleDetailPanel.h"

#include "app/Theme.h"
#include "app/UiHelpers.h"
#include "vision/HeightMap3DFrame.h"

#include <QHBoxLayout>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>

namespace {

// 从整板点云里裁出以路径（粒子）为中心的一块，避免右侧 3D 视图显示整块铜板。
// u/v 是扫描图的像素坐标，xScaleMm / yScaleMm 只影响显示比例，裁剪仍按像素进行。
// TODO(粒子数据): 这是占位实现。算法侧已有粒子识别结果，但尚未通过 ROS topic 下发，
// 目前只能用铣削路径的 u/v 包围盒近似粒子范围。算法数据接入后，把入参换成粒子 bbox，
// 并同步替换 ImageViewFrame 里基于路径线的命中测试。详见 README「粒子信息与粒子三维点云」。
PointCloudFrame cropParticleCloud(const PointCloudFrame &frame, const MillingPathVM &path)
{
    if (!frame.hasFloatRange()) {
        return PointCloudFrame();
    }
    const int w = int(frame.width);
    const int h = int(frame.height);
    const int u0 = std::min(path.uStart, path.uEnd);
    const int u1 = std::max(path.uStart, path.uEnd);
    const int v0 = std::min(path.vStart, path.vEnd);
    const int v1 = std::max(path.vStart, path.vEnd);

    // 粒子半径留白：至少 16 px，长路径按跨度的一半扩展。
    const int span = std::max(u1 - u0, v1 - v0);
    const int margin = std::max(16, span / 2);
    const int x0 = std::clamp(u0 - margin, 0, w - 1);
    const int x1 = std::clamp(u1 + margin, 0, w - 1);
    const int y0 = std::clamp(v0 - margin, 0, h - 1);
    const int y1 = std::clamp(v1 + margin, 0, h - 1);
    const int cw = x1 - x0 + 1;
    const int ch = y1 - y0 + 1;
    if (cw < 8 || ch < 8) {
        return PointCloudFrame();
    }

    PointCloudFrame out;
    out.frameId = frame.frameId;
    out.taskId = frame.taskId;
    out.timestampNs = frame.timestampNs;
    out.width = quint32(cw);
    out.height = quint32(ch);
    out.rangePixelFormat = frame.rangePixelFormat;
    out.intensityPixelFormat = frame.intensityPixelFormat;
    out.hasMetricGeometry = frame.hasMetricGeometry;
    out.xScaleMm = frame.xScaleMm;
    out.yScaleMm = frame.yScaleMm;
    out.missingValue = frame.missingValue;
    out.encoderResolutionMm = frame.encoderResolutionMm;
    // 保留绝对坐标，方便和 2D 位置对应。
    out.xOffsetMm = frame.xOffsetMm + double(x0) * frame.xScaleMm;
    out.yOffsetMm = frame.yOffsetMm + double(y0) * frame.yScaleMm;
    if (!frame.encoderValues.isEmpty() && frame.encoderValues.size() == h) {
        out.encoderValues = frame.encoderValues.mid(y0, ch);
    }

    out.rangeFloat32.resize(int(std::size_t(cw) * std::size_t(ch) * sizeof(float)));
    const float *src = reinterpret_cast<const float *>(frame.rangeFloat32.constData());
    float *dst = reinterpret_cast<float *>(out.rangeFloat32.data());
    for (int row = 0; row < ch; ++row) {
        std::memcpy(dst + std::size_t(row) * std::size_t(cw),
                    src + std::size_t(y0 + row) * std::size_t(w) + std::size_t(x0),
                    std::size_t(cw) * sizeof(float));
    }
    return out;
}

class ParticlePreview : public QWidget
{
public:
    explicit ParticlePreview(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setMinimumSize(180, 140);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        m_cloud = new HeightMap3DFrame(this);
        m_cloud->setObjectName(QStringLiteral("heightMap3dArea"));
        m_cloud->setToolTip(QStringLiteral("左键拖动旋转，滚轮缩放"));
        layout->addWidget(m_cloud);
    }

    HeightMap3DFrame *cloud() const { return m_cloud; }

    QSize sizeHint() const override { return QSize(240, 180); }

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

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto *panel = createPanel(QStringLiteral("粒子三维点云"), nullptr, this);
    root->addWidget(panel);
    auto *panelLayout = qobject_cast<QVBoxLayout *>(panel->layout());

    auto *bodyHost = new QWidget(panel);
    bodyHost->setObjectName(QStringLiteral("particleBody"));
    auto *body = new QHBoxLayout(bodyHost);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(8);

    auto *preview = new ParticlePreview(bodyHost);
    m_preview = preview;
    m_heightMap = preview->cloud();
    body->addWidget(preview, 1);
    panelLayout->addWidget(bodyHost, 1);

    m_sideHost = new QWidget(bodyHost);
    m_sideHost->hide();
    m_sideHost->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    m_sideLayout = new QVBoxLayout(m_sideHost);
    m_sideLayout->setContentsMargins(0, 0, 0, 0);
    m_sideLayout->setSpacing(8);

    clear();
    Theme::watch(this, [this]() { refresh(); });
}

void ParticleDetailPanel::setPointCloudFrame(const PointCloudFrame &frame)
{
    m_frame = frame;
    // 只保存整板点云；具体显示哪一块由当前选中的粒子决定。
    refreshParticleCloud();
    refresh();
}

void ParticleDetailPanel::setMillingPaths(const QVector<MillingPathVM> &paths)
{
    m_paths = paths;
    m_selected = -1;
    m_completed = 0;
    m_current = 0;
    refreshParticleCloud();
    refresh();
}

void ParticleDetailPanel::refreshParticleCloud()
{
    PointCloudFrame particleFrame;
    if (m_selected >= 0 && m_selected < m_paths.size()) {
        particleFrame = cropParticleCloud(m_frame, m_paths[m_selected]);
    }
    if (auto *preview = static_cast<ParticlePreview *>(m_preview)) {
        preview->setFrame(particleFrame);
    } else if (m_heightMap) {
        if (particleFrame.hasFloatRange()) {
            m_heightMap->setPointCloudFrame(particleFrame);
        } else {
            m_heightMap->clearPointCloud();
        }
    }
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
    refreshParticleCloud();
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
    if (QWidget *bodyHost = findChild<QWidget *>(QStringLiteral("particleBody"))) {
        if (auto *body = qobject_cast<QHBoxLayout *>(bodyHost->layout())) {
            if (body->indexOf(m_sideHost) < 0) {
                body->addWidget(m_sideHost, 1);
            }
        }
    }
    m_sideHost->show();
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
    refreshParticleCloud();
    setTaskProgress(0.0f);
    refresh();
}

void ParticleDetailPanel::refresh()
{
}
