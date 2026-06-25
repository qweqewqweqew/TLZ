#ifndef RUNDATAPANEL_H
#define RUNDATAPANEL_H

#include <QFrame>
#include <QHash>

class QLabel;

class RunDataPanel : public QFrame
{
    Q_OBJECT

public:
    explicit RunDataPanel(QWidget *parent = nullptr);

    void setMetricValue(const QString &key, const QString &value);

private:
    QWidget *createMetricCard(const QString &key,
                              const QString &title,
                              const QString &value,
                              const QString &unit,
                              const QColor &accent,
                              QWidget *parent);

    QHash<QString, QLabel *> m_metricLabels;
};

#endif // RUNDATAPANEL_H
