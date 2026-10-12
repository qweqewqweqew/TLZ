#ifndef HEIGHTMAP3DFRAME_H
#define HEIGHTMAP3DFRAME_H

#include "vision/PointCloudFrame.h"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QVector3D>

#include <memory>

class QMouseEvent;
class QOpenGLShaderProgram;
class QShowEvent;
class QWheelEvent;

class HeightMap3DFrame : public QOpenGLWidget, protected QOpenGLFunctions
{
public:
    explicit HeightMap3DFrame(QWidget *parent = nullptr);
    ~HeightMap3DFrame() override;

    void setPointCloudFrame(const PointCloudFrame &frame);
    void clearPointCloud();

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    struct Vertex
    {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
        float heightNorm{0.0f};
    };

    void rebuildVertices();
    void uploadVertices();
    void resetView();
    QMatrix4x4 currentViewProjection() const;
    QVector3D cameraPosition() const;
    QVector3D cursorAnchor(const QPointF &position);
    QVector3D targetPlaneAnchor(const QPointF &position) const;

    PointCloudFrame m_frame;
    QVector<Vertex> m_vertices;
    QOpenGLBuffer m_vertexBuffer{QOpenGLBuffer::VertexBuffer};
    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QMatrix4x4 m_lastViewProjection;
    QVector3D m_boundsMin;
    QVector3D m_boundsMax;
    QVector3D m_target;
    quint64 m_validPointCount{0};
    float m_sceneDiagonal{1.0f};
    float m_distance{1.0f};
    float m_yaw{-0.78f};
    float m_pitch{0.62f};
    QPoint m_lastMousePosition;
    QString m_renderError;
    bool m_dragging{false};
    bool m_glReady{false};
    bool m_uploadPending{false};
    bool m_flipX{false};
    int m_cropLeftColumns{0};
    int m_cropRightColumns{0};
};

#endif // HEIGHTMAP3DFRAME_H
