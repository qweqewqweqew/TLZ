#ifndef HIKPREVIEWDIALOG_H
#define HIKPREVIEWDIALOG_H

#include <QDialog>
#include <QtGui/qwindowdefs.h>

class HikLiveView;

class HikPreviewDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HikPreviewDialog(QWidget *parent = nullptr);

    WId videoWindowId();
    void setConnectionStatus(bool connected, const QString &message);

signals:
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    HikLiveView *m_liveView{nullptr};
};

#endif // HIKPREVIEWDIALOG_H
