#include "ui/settings/ProgramParamDisplay.h"

#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QLabel>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QWidget>

namespace {

QString algoRootFromStartAll()
{
    QDir dir(QCoreApplication::applicationDirPath());
    for (int depth = 0; depth < 4; ++depth) {
        const QStringList bats{
            dir.filePath(QStringLiteral("start_all.bat")),
            dir.filePath(QStringLiteral("config/start_all.bat")),
        };
        for (const QString &bat : bats) {
            QFile file(bat);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            while (!file.atEnd()) {
                const QString line = QString::fromLocal8Bit(file.readLine()).trimmed();
                const int eq = line.indexOf(QLatin1Char('='));
                if (eq < 0 || !line.startsWith(QStringLiteral("set"), Qt::CaseInsensitive)) {
                    continue;
                }
                const QString name = line.left(eq).mid(3).trimmed().remove(QLatin1Char('"'));
                if (name.compare(QStringLiteral("ALGO_ROOT"), Qt::CaseInsensitive) != 0) {
                    continue;
                }
                QString value = line.mid(eq + 1).trimmed();
                if (value.startsWith(QLatin1Char('"'))) {
                    value.remove(0, 1);
                }
                if (value.endsWith(QLatin1Char('"'))) {
                    value.chop(1);
                }
                return value.trimmed();
            }
        }
        if (!dir.cdUp()) {
            break;
        }
    }
    return QString();
}

QString algorithmConfigFile(const QString &fileName)
{
    const QByteArray env = qgetenv("ROBOT_CLIENT_ROOT");
    if (!env.isEmpty()) {
        const QString path = QDir(QString::fromLocal8Bit(env))
                                 .filePath(QStringLiteral("copper_pkg/configs/") + fileName);
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    const QString root = algoRootFromStartAll();
    if (root.isEmpty()) {
        return QString();
    }
    return QDir(root).filePath(QStringLiteral("copper_pkg/configs/") + fileName);
}

bool readJsonObject(const QString &path, QJsonObject *object, QString *status, const QString &title)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        *status = QStringLiteral("%1 · 未找到").arg(title);
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *status = QStringLiteral("%1 · 无法打开 %2")
                      .arg(title, QDir::toNativeSeparators(path));
        return false;
    }
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        *status = QStringLiteral("%1 · 无法读取 %2")
                      .arg(title, QDir::toNativeSeparators(path));
        return false;
    }
    *object = document.object();
    *status = QStringLiteral("%1 · 只读　｜　%2").arg(title, QDir::toNativeSeparators(path));
    return true;
}

bool readMatrix(const QJsonObject &object, const QString &key, QVector<double> *cells)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray() || value.toArray().size() != 4) {
        return false;
    }
    QVector<double> parsed;
    parsed.reserve(16);
    const QJsonArray rows = value.toArray();
    for (int row = 0; row < 4; ++row) {
        if (!rows.at(row).isArray() || rows.at(row).toArray().size() != 4) {
            return false;
        }
        const QJsonArray columns = rows.at(row).toArray();
        for (int column = 0; column < 4; ++column) {
            if (!columns.at(column).isDouble()) {
                return false;
            }
            parsed.append(columns.at(column).toDouble());
        }
    }
    *cells = parsed;
    return true;
}

QString formatNumber(double value);

QString jsonNumberText(const QJsonValue &value)
{
    if (!value.isDouble()) {
        return QStringLiteral("—");
    }
    return formatNumber(value.toDouble());
}

void fillCalibration(QWidget *page)
{
    auto *table = page->findChild<QTableWidget *>(QStringLiteral("craftTable"));
    auto *pointsStatus = page->findChild<QLabel *>(QStringLiteral("craftPointsStatus"));
    auto *matrixStatus = page->findChild<QLabel *>(QStringLiteral("craftMatrixStatus"));
    auto *calibrationState = page->findChild<QLabel *>(QStringLiteral("craftCalibrationState"));
    auto *matrixFrame = page->findChild<QFrame *>(QStringLiteral("craftMatrix"));
    const auto matrixCells = matrixFrame
                                 ? matrixFrame->findChildren<QLabel *>(QStringLiteral("craftMatrixCell"),
                                                                       Qt::FindDirectChildrenOnly)
                                 : QList<QLabel *>();
    if (!table && matrixCells.isEmpty()) {
        return;
    }

    const QString pointsPath = algorithmConfigFile(QStringLiteral("camera_robot_calibration_points.json"));
    const QString cameraPath = algorithmConfigFile(QStringLiteral("camera_param.json"));
    QJsonObject pointsObject;
    QJsonObject cameraObject;
    QString pointsMessage;
    QString cameraMessage;
    const bool pointsLoaded = readJsonObject(pointsPath, &pointsObject, &pointsMessage,
                                              QStringLiteral("坐标点对"));
    const bool cameraLoaded = readJsonObject(cameraPath, &cameraObject, &cameraMessage,
                                              QStringLiteral("坐标转换矩阵"));

    if (table) {
        table->clearContents();
        table->setRowCount(0);
        const QJsonArray samples = pointsLoaded ? pointsObject.value(QStringLiteral("samples")).toArray()
                                                : QJsonArray();
        table->setRowCount(samples.size());
        for (int row = 0; row < samples.size(); ++row) {
            const QJsonObject sample = samples.at(row).toObject();
            const QStringList texts{
                QString::number(row + 1),
                jsonNumberText(sample.value(QStringLiteral("u"))),
                jsonNumberText(sample.value(QStringLiteral("v"))),
                jsonNumberText(sample.value(QStringLiteral("depth"))),
                jsonNumberText(sample.value(QStringLiteral("robot_x"))),
                jsonNumberText(sample.value(QStringLiteral("robot_y"))),
                jsonNumberText(sample.value(QStringLiteral("robot_z"))),
                QStringLiteral("—"),
            };
            for (int column = 0; column < texts.size(); ++column) {
                auto *item = new QTableWidgetItem(texts.at(column));
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                item->setTextAlignment(Qt::AlignCenter);
                table->setItem(row, column, item);
            }
        }
    }
    if (pointsStatus) {
        if (!pointsLoaded) {
            pointsStatus->setText(pointsMessage);
        } else if (!pointsObject.value(QStringLiteral("samples")).isArray()
                   || pointsObject.value(QStringLiteral("samples")).toArray().isEmpty()) {
            pointsStatus->setText(QStringLiteral("坐标点对 · 文件里没有点　｜　%1")
                                      .arg(QDir::toNativeSeparators(pointsPath)));
        } else if (pointsObject.value(QStringLiteral("dataset_type")).toString()
                   == QStringLiteral("synthetic_verification")) {
            pointsStatus->setText(QStringLiteral("只读 · 合成校验点，不是现场实测 · 残差没有字段　｜　%1")
                                      .arg(QDir::toNativeSeparators(pointsPath)));
        } else {
            pointsStatus->setText(QStringLiteral("%1 · 残差没有字段").arg(pointsMessage));
        }
    }

    QVector<double> cells;
    QString matrixName;
    bool matrixLoaded = false;
    if (cameraLoaded && cameraObject.value(QStringLiteral("use_pix_to_robot_matrix")).isBool()) {
        const bool usePixel = cameraObject.value(QStringLiteral("use_pix_to_robot_matrix")).toBool();
        matrixName = usePixel ? QStringLiteral("pix_to_robot_matrix")
                              : QStringLiteral("camera_to_robot_matrix");
        matrixLoaded = readMatrix(cameraObject, matrixName, &cells);
    }
    for (int index = 0; index < matrixCells.size(); ++index) {
        const QString text = matrixLoaded && index < cells.size()
                                 ? formatNumber(cells.at(index))
                                 : QStringLiteral("—");
        matrixCells.at(index)->setText(text);
    }
    if (matrixStatus) {
        if (!cameraLoaded) {
            matrixStatus->setText(cameraMessage);
        } else if (!cameraObject.value(QStringLiteral("use_pix_to_robot_matrix")).isBool()) {
            matrixStatus->setText(QStringLiteral("未标定 · 没有 use_pix_to_robot_matrix"));
        } else if (!matrixLoaded) {
            matrixStatus->setText(QStringLiteral("无法读取 %1").arg(matrixName));
        } else {
            matrixStatus->setText(QStringLiteral("只读 · %1 · %2")
                                      .arg(matrixName, QDir::toNativeSeparators(cameraPath)));
        }
    }
    if (calibrationState) {
        calibrationState->setText(QStringLiteral("坐标点对和矩阵只读。扫描点云来自 SICK 扫图，只显示。转换模式和残差没有来源，保持占位。"));
    }
}

QString formatNumber(double value)
{
    const qint64 rounded = qRound64(value);
    if (qAbs(value - double(rounded)) < 1e-9) {
        return QString::number(rounded);
    }
    return QString::number(value, 'g', 8);
}

QString formatScalar(const QString &key, const QJsonValue &value)
{
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("开") : QStringLiteral("关");
    }
    if (value.isString()) {
        const QString text = value.toString();
        if (key == QStringLiteral("target_extraction_method")) {
            if (text == QStringLiteral("bspline")) {
                return QStringLiteral("B 样条");
            }
            if (text == QStringLiteral("plane")) {
                return QStringLiteral("局部平面");
            }
        }
        if (key == QStringLiteral("grinding_pass_strategy")) {
            if (text == QStringLiteral("per_run")) {
                return QStringLiteral("按扫描段");
            }
            if (text == QStringLiteral("by_layer")) {
                return QStringLiteral("按层");
            }
        }
        return text.isEmpty() ? QStringLiteral("未获取") : text;
    }
    if (!value.isDouble()) {
        return QStringLiteral("未获取");
    }
    const double number = value.toDouble();
    if (key == QStringLiteral("stepover") && number <= 0.0) {
        return QStringLiteral("自动");
    }
    if (key == QStringLiteral("edge_feather_depth_step_mm") && qAbs(number) < 1e-9) {
        return QStringLiteral("自动");
    }
    if (key == QStringLiteral("grinding_strictness_level")) {
        if (qAbs(number - 1.0) < 1e-9) {
            return QStringLiteral("保留已提取粒子");
        }
        if (qAbs(number - 2.0) < 1e-9) {
            return QStringLiteral("剔除孤立小块");
        }
    }
    return formatNumber(number);
}

QString formatField(const QJsonObject &object, const QString &key)
{
    const QStringList parts = key.split(QStringLiteral(" / "), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return QStringLiteral("未获取");
    }
    QStringList values;
    values.reserve(parts.size());
    for (const QString &part : parts) {
        if (!object.contains(part)) {
            return QStringLiteral("未获取");
        }
        const QString text = formatScalar(part, object.value(part));
        if (text == QStringLiteral("未获取")) {
            return text;
        }
        values.append(text);
    }
    if (parts.size() == 1) {
        return values.first();
    }
    if (parts.first().startsWith(QStringLiteral("region_"))) {
        return QStringLiteral("(%1, %2) – (%3, %4)")
            .arg(values.value(0), values.value(1), values.value(2), values.value(3));
    }
    if (parts.first().startsWith(QStringLiteral("robot_wait_"))) {
        return QStringLiteral("(%1, %2, %3)")
            .arg(values.value(0), values.value(1), values.value(2));
    }
    return values.join(QStringLiteral("，"));
}

class ProgramParamBinder : public QObject
{
public:
    explicit ProgramParamBinder(QWidget *page)
        : QObject(page)
        , m_page(page)
    {
        page->installEventFilter(this);
        connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &) {
            reload();
        });
        connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
            reload();
        });
        reload();
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_page && event->type() == QEvent::Show) {
            reload();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void reload()
    {
        const QString path = algorithmConfigFile(QStringLiteral("program_param.json"));
        const QString pointsPath = algorithmConfigFile(QStringLiteral("camera_robot_calibration_points.json"));
        const QString cameraPath = algorithmConfigFile(QStringLiteral("camera_param.json"));
        QJsonObject object;
        QString status = QStringLiteral("算法配置 · 只读　｜　未找到 program_param.json　｜　不显示默认数值");
        const bool loaded = readJsonObject(path, &object, &status, QStringLiteral("算法配置"));
        if (!loaded && status == QStringLiteral("算法配置 · 未找到")) {
            status = QStringLiteral("算法配置 · 只读　｜　未找到 program_param.json　｜　不显示默认数值");
        }

        QStringList watchPaths;
        QString watchDir;
        const QStringList files{path, pointsPath, cameraPath};
        for (const QString &file : files) {
            if (file.isEmpty()) {
                continue;
            }
            const QString folder = QFileInfo(file).absolutePath();
            if (watchDir.isEmpty() && QFileInfo::exists(folder)) {
                watchDir = folder;
            }
            if (QFileInfo::exists(file)) {
                watchPaths.append(QFileInfo(file).absoluteFilePath());
            }
        }
        watchPaths.removeDuplicates();
        if (m_watchedFiles != watchPaths || m_watchedDir != watchDir) {
            const QStringList previous = m_watcher.files() + m_watcher.directories();
            if (!previous.isEmpty()) {
                m_watcher.removePaths(previous);
            }
            m_watchedFiles = watchPaths;
            m_watchedDir = watchDir;
            if (!watchDir.isEmpty()) {
                m_watcher.addPath(watchDir);
            }
            if (!watchPaths.isEmpty()) {
                m_watcher.addPaths(watchPaths);
            }
        }

        const auto values = m_page->findChildren<QLabel *>(QStringLiteral("craftReadOnlyValue"));
        for (QLabel *label : values) {
            const QString key = label->property("algorithmJsonKey").toString();
            label->setText(loaded ? formatField(object, key) : QStringLiteral("未获取"));
        }
        const auto states = m_page->findChildren<QLabel *>(QStringLiteral("craftAlgorithmState"));
        for (QLabel *state : states) {
            state->setText(status);
        }
        fillCalibration(m_page);
    }

    QWidget *m_page = nullptr;
    QFileSystemWatcher m_watcher;
    QStringList m_watchedFiles;
    QString m_watchedDir;
};

} // namespace

void attachProgramParamDisplay(QWidget *page)
{
    if (!page) {
        return;
    }
    new ProgramParamBinder(page);
}
