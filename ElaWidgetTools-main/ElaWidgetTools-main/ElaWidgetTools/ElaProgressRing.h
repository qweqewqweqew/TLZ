#ifndef ELAFRAMEWORK_ELAWIDGETTOOLS_INCLUDE_ELAPROGRESSRING_H_
#define ELAFRAMEWORK_ELAWIDGETTOOLS_INCLUDE_ELAPROGRESSRING_H_

#include "ElaDef.h"

#include <QColor>
#include <QWidget>

class ElaProgressRingPrivate;
class ELA_EXPORT ElaProgressRing : public QWidget
{
    Q_OBJECT
    Q_Q_CREATE(ElaProgressRing);
    Q_PROPERTY_CREATE_Q_H(bool, IsBusying)
    Q_PROPERTY_CREATE_Q_H(bool, IsTransparent)
    Q_PROPERTY_CREATE_Q_H(bool, IsDisplayValue)
    Q_PROPERTY_CREATE_Q_H(ElaProgressRingType::ValueDisplayMode, ValueDisplayMode)
    Q_PROPERTY_CREATE_Q_H(int, BusyingWidth)
    Q_PROPERTY_CREATE_Q_H(int, BusyingDurationTime)
    Q_PROPERTY_CREATE_Q_H(int, Minimum)
    Q_PROPERTY_CREATE_Q_H(int, Maximum)
    Q_PROPERTY_CREATE_Q_H(int, Value)
    Q_PROPERTY_CREATE_Q_H(int, ValuePixelSize)
public:
    explicit ElaProgressRing(QWidget* parent = nullptr);
    ~ElaProgressRing() override;

    void setRange(int min, int max);
    void setProgressColor(const QColor& color);
    QColor getProgressColor() const;
    void setTrackColor(const QColor& color);
    QColor getTrackColor() const;
    void setValueColor(const QColor& color);
    QColor getValueColor() const;
    void setIsSegmentedArc(bool isSegmentedArc);
    bool getIsSegmentedArc() const;
    void setSegmentCount(int segmentCount);
    int getSegmentCount() const;

Q_SIGNALS:
    Q_SIGNAL void rangeChanged(int min, int max);

protected:
    virtual void paintEvent(QPaintEvent* event) override;
};

#endif //ELAFRAMEWORK_ELAWIDGETTOOLS_INCLUDE_ELAPROGRESSRING_H_
