#ifndef VISIONSETTINGSPAGE_H
#define VISIONSETTINGSPAGE_H

#include "ui/settings/SettingsSubPage.h"

#include <QDateTime>
#include <QtGui/qwindowdefs.h>

class ElaPushButton;
class QComboBox;
class QFrame;
class QLabel;
class QShowEvent;
class QHideEvent;

class VisionSettingsPage : public SettingsSubPage
{
    Q_OBJECT

public:
    explicit VisionSettingsPage(QWidget *parent = nullptr);

    QString bindingSummary() const override;
    bool isEditable() const override { return false; }

public slots:
    void setCameraNodeStatus(bool connected, const QString &detail);
    void setCameraTopicDiagnostics(bool bridgeRunning, int publisherCount);
    void setHikCameraStatus(bool connected, const QString &detail);

signals:
    void previewRequested(WId targetWindow);
    void stopPreviewRequested();
    void refreshCameraDiagnosticsRequested();
    void processSettingsRequested();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void updateNodeStatus();
    void updatePreviewStatus();
    void refreshCalibrationValues();


    QLabel *m_nodeBadge{nullptr};
    QLabel *m_publisherCount{nullptr};
    QLabel *m_diagnosticBanner{nullptr};
    ElaPushButton *m_diagnosticsButton{nullptr};
    ElaPushButton *m_refreshNodeButton{nullptr};

    QFrame *m_previewSurface{nullptr};
    QLabel *m_previewPlaceholder{nullptr};
    QLabel *m_previewBadge{nullptr};
    QComboBox *m_cameraSelector{nullptr};
    ElaPushButton *m_startPreviewButton{nullptr};
    ElaPushButton *m_stopPreviewButton{nullptr};

    QLabel *m_calibrationBadge{nullptr};
    QLabel *m_compensationX{nullptr};
    QLabel *m_compensationY{nullptr};
    QLabel *m_compensationZ{nullptr};

    bool m_cameraNodeReceived{false};
    bool m_cameraNodeConnected{false};
    bool m_bridgeRunning{false};
    bool m_hikConnected{false};
    bool m_hikConnecting{false};
    bool m_previewRequested{false};
    int m_publisherCountValue{-1};
    QDateTime m_cameraLastMessageAt;
    QString m_cameraDetail;
    QString m_hikDetail;
};

#endif // VISIONSETTINGSPAGE_H
