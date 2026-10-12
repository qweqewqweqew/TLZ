#ifndef VERTICALGAUGE_H
#define VERTICALGAUGE_H

#include <QColor>
#include <QWidget>

class QRectF;
class QVariantAnimation;

class VerticalGauge : public QWidget
{
public:
    explicit VerticalGauge(QWidget *parent = nullptr);

    void setRange(double minimum, double maximum);
    void setValue(double value);
    void clearValue();
    void setTitle(const QString &title);
    void setUnit(const QString &unit);
    void setStatusText(const QString &text, const QColor &color);
    void setValueDecimals(int decimals);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    double valueToY(double value, const QRectF &track) const;
    QString formattedValue(double value, bool tickLabel = false) const;
    void updateAccessibleText();

private:
    QString m_title;
    QString m_unit;
    QString m_statusText;
    QColor m_statusColor;
    double m_minimum{0.0};
    double m_maximum{100.0};
    double m_value{0.0};
    double m_displayedValue{0.0};
    int m_valueDecimals{-1};
    bool m_hasValue{false};
    QVariantAnimation *m_animation{nullptr};
};

#endif // VERTICALGAUGE_H
