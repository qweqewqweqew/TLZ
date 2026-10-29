#ifndef CRAFTPLACEHOLDER_H
#define CRAFTPLACEHOLDER_H

#include <QString>
#include <QStringList>

class QButtonGroup;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QSpinBox;
class QVBoxLayout;
class QWidget;

// 检测与标定、修磨工艺两页共用的占位卡片。数值只在界面里联动，不写文件。

QString craftPageStyle();

QVBoxLayout *craftCard(QWidget *parent,
                       QVBoxLayout *column,
                       const QString &title,
                       const QString &subtitle);

QSpinBox *craftInt(QWidget *parent, int value, int minValue, int maxValue);
QDoubleSpinBox *craftDouble(QWidget *parent,
                            double value,
                            int decimals,
                            double minValue,
                            double maxValue,
                            double step);

QWidget *craftRow(QWidget *parent, const QString &label, QWidget *editor, const QString &unit);

QButtonGroup *craftSegments(QWidget *parent,
                            QVBoxLayout *body,
                            const QStringList &labels,
                            int selected);

QLabel *craftNote(QWidget *parent, const QString &text);

// 算法只读参数行。jsonKey 保存在值标签属性中，供后续 program_param.json 读取器回填。
QFrame *craftReadOnlyRow(QWidget *parent,
                         const QString &label,
                         const QString &description,
                         const QString &jsonKey,
                         const QString &unit = QString());

#endif // CRAFTPLACEHOLDER_H
