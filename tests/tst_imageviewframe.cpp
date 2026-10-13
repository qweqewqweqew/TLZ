#include "vision/ImageViewFrame.h"

#include <QElapsedTimer>
#include <QEvent>
#include <QImage>
#include <QThread>
#include <QWheelEvent>
#include <QtTest>

namespace {

// Widget 202x202 -> viewport rect().adjusted(1, 1, -2, -2) is 199x199 at (1, 1),
// so a 199x199 image is drawn 1:1 and image (u, v) lands on widget (u + 1, v + 1).
constexpr int kWidgetSize = 202;
constexpr int kImageSize = 199;

PointCloudFrame unitMetricFrame()
{
    PointCloudFrame frame;
    frame.width = kImageSize;
    frame.height = kImageSize;
    frame.hasMetricGeometry = true;
    frame.xScaleMm = 1.0;
    frame.yScaleMm = 1.0;
    return frame;
}

QImage blackImage()
{
    QImage image(kImageSize, kImageSize, QImage::Format_Grayscale8);
    image.fill(0);
    return image;
}

class PaintCounter : public QObject
{
public:
    int paints{0};

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) {
            ++paints;
        }
        return QObject::eventFilter(watched, event);
    }
};

bool isCutYellow(QRgb c)
{
    return qRed(c) > 200 && qGreen(c) > 150 && qBlue(c) < 100;
}

// Pixels on a column tinted by the translucent accent grind stroke over black.
int grindThicknessAt(const QImage &shot, int x)
{
    int count = 0;
    for (int y = 0; y < shot.height(); ++y) {
        const QRgb c = shot.pixel(x, y);
        if (qBlue(c) > 30 && qRed(c) < qBlue(c)) {
            ++count;
        }
    }
    return count;
}

} // namespace

class TestImageViewFrame : public QObject
{
    Q_OBJECT

private slots:
    void newPathsDropPreviousProgress();
    void clearPathsDropsProgress();
    void poseRepaintsAreThrottled();
    void poseRepaintStillHappensAfterBurst();
    void grindWidthFollowsZoomLikeImage();
};

void TestImageViewFrame::newPathsDropPreviousProgress()
{
    ImageViewFrame view;
    view.resize(kWidgetSize, kWidgetSize);
    view.setImage(blackImage());
    const QVector<MillingPathVM> paths{{20, 100, 180, 100, 0.0f, 0.0f, 0.0f, true}};
    view.setMillingPaths(paths);
    QVERIFY(isCutYellow(view.grab().toImage().pixel(100, 101)));

    // Path 1 completed on the previous task: drawn grey, not cut-yellow.
    view.setPathProgress(1, 0);
    QVERIFY(!isCutYellow(view.grab().toImage().pixel(100, 101)));

    // A new task arrives before PLC reports on it: must not inherit "completed".
    view.setMillingPaths(paths);
    QVERIFY(isCutYellow(view.grab().toImage().pixel(100, 101)));
}

void TestImageViewFrame::clearPathsDropsProgress()
{
    ImageViewFrame view;
    view.resize(kWidgetSize, kWidgetSize);
    view.setImage(blackImage());
    const QVector<MillingPathVM> paths{{20, 100, 180, 100, 0.0f, 0.0f, 0.0f, true}};
    view.setMillingPaths(paths);
    view.setPathProgress(1, 0);
    view.clearMillingPaths();
    view.setMillingPaths(paths);
    QVERIFY(isCutYellow(view.grab().toImage().pixel(100, 101)));
}

void TestImageViewFrame::poseRepaintsAreThrottled()
{
    ImageViewFrame view;
    view.resize(kWidgetSize, kWidgetSize);
    view.setImage(blackImage());
    view.setMetricFrame(unitMetricFrame());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));
    QTest::qWait(50);

    PaintCounter counter;
    view.installEventFilter(&counter);

    // ~1 kHz PLC feedback for 330 ms, event loop serviced between samples.
    QElapsedTimer clock;
    clock.start();
    int sample = 0;
    while (clock.elapsed() < 330) {
        view.setToolPose(20.0 + (sample++ % 150), 100.0, 0.0, true);
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    QTest::qWait(60);

    qInfo("paints=%d samples=%d", counter.paints, sample);
    QVERIFY2(sample > 100, qPrintable(QString::number(sample)));
    // 330 ms at a 33 ms cap is ~10 repaints; allow slack for scheduler jitter.
    QVERIFY2(counter.paints <= 14,
             qPrintable(QStringLiteral("paints=%1 for %2 samples")
                            .arg(counter.paints)
                            .arg(sample)));
    QVERIFY2(counter.paints >= 4, qPrintable(QString::number(counter.paints)));
}

void TestImageViewFrame::poseRepaintStillHappensAfterBurst()
{
    ImageViewFrame view;
    view.resize(kWidgetSize, kWidgetSize);
    view.setImage(blackImage());
    view.setMetricFrame(unitMetricFrame());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    view.setToolPose(50.0, 100.0, 0.0, true);
    QTest::qWait(5);
    PaintCounter counter;
    view.installEventFilter(&counter);
    // Inside the throttle window: the last pose must still be painted (deferred).
    view.setToolPose(120.0, 100.0, 0.0, true);
    QTRY_VERIFY_WITH_TIMEOUT(counter.paints >= 1, 200);

    const QImage shot = view.grab().toImage();
    const QRgb tip = shot.pixel(121, 101);
    QVERIFY2(qBlue(tip) > 100 || qRed(tip) > 150, "tool disc not drawn at latest pose");
}

void TestImageViewFrame::grindWidthFollowsZoomLikeImage()
{
    ImageViewFrame view;
    view.resize(kWidgetSize, kWidgetSize);
    view.setImage(blackImage());
    view.setMetricFrame(unitMetricFrame());
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    for (int u = 20; u <= 100; u += 2) {
        view.setToolPose(double(u), 100.0, -2.5, true, /*leaveGrindTrace=*/true);
    }
    QTest::qWait(60);
    const int widthAtFit = grindThicknessAt(view.grab().toImage(), 31);
    QVERIFY2(widthAtFit >= 16 && widthAtFit <= 24, qPrintable(QString::number(widthAtFit)));

    // Zoom in 2 wheel steps around the widget centre (1.15^2 = 1.3225x).
    const QPointF centre(kWidgetSize / 2.0, kWidgetSize / 2.0);
    for (int i = 0; i < 2; ++i) {
        QWheelEvent wheel(centre, view.mapToGlobal(centre.toPoint()), QPoint(), QPoint(0, 120),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&view, &wheel);
    }
    QTest::qWait(60);
    // Next cut sample rebuilds the whole layer at the current zoom.
    view.setToolPose(102.0, 100.0, -2.5, true, true);
    QTest::qWait(60);

    // Image u=30 now sits at widget x = 100 + (30 + 1 - 100) * 1.3225 ~= 9.
    const int widthZoomed = grindThicknessAt(view.grab().toImage(), 10);
    const double ratio = double(widthZoomed) / double(widthAtFit);
    qInfo("fit=%d zoomed=%d ratio=%.3f", widthAtFit, widthZoomed, ratio);
    QVERIFY2(ratio > 1.15 && ratio < 1.5,
             qPrintable(QStringLiteral("fit=%1 zoomed=%2 ratio=%3")
                            .arg(widthAtFit)
                            .arg(widthZoomed)
                            .arg(ratio)));
}

QTEST_MAIN(TestImageViewFrame)
#include "tst_imageviewframe.moc"
