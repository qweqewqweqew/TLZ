#include "ui/settings/CalibrationPlaceholderPage.h"

#include "app/Theme.h"
#include "ui/settings/CraftPlaceholder.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QWidget *createAlgorithmPage(QWidget *parent)
{
    auto *page = new QWidget(parent);
    page->setObjectName(QStringLiteral("craftContent"));
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(page);
    scroll->setObjectName(QStringLiteral("craftScroll"));
    scroll->viewport()->setObjectName(QStringLiteral("craftViewport"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    outer->addWidget(scroll);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("craftContent"));
    content->setMinimumWidth(1100);
    scroll->setWidget(content);
    auto *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 10);
    layout->setSpacing(12);
    auto *state = craftNote(content,
        QStringLiteral("算法配置 · 只读　｜　来源：program_param.json（实际路径待提供）　｜　未读取前不显示任何默认数值"));
    state->setObjectName(QStringLiteral("craftAlgorithmState"));
    layout->addWidget(state);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->setSpacing(14);
    right->setSpacing(14);
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    layout->addLayout(columns, 1);

    auto *basic = craftCard(content, left, QStringLiteral("目标提取与粒子筛选"),
                            QStringLiteral("算法如何认定待磨粒子，以及孤立小块的剔除规则"));
    const auto addBasic = [basic, content](const QString &name, const QString &description,
                                           const QString &key, const QString &unit = QString()) {
        basic->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addBasic(QStringLiteral("目标提取方式"), QStringLiteral("B 样条基准面残差法或局部平面拟合法"),
             QStringLiteral("target_extraction_method"));
    addBasic(QStringLiteral("粒子凸起高度阈值"), QStringLiteral("高于基准面多少才识别为待磨粒子"),
             QStringLiteral("targetsize_height_thresh"), QStringLiteral("mm"));
    addBasic(QStringLiteral("平面模式聚类距离"), QStringLiteral("相邻高点合并为同一打磨区域的最大距离"),
             QStringLiteral("kmeans_distance"), QStringLiteral("mm"));
    addBasic(QStringLiteral("B 样条最小粒子面积"), QStringLiteral("小于该面积的连通区域作为噪声丢弃"),
             QStringLiteral("bspline_min_particle_area_mm2"), QStringLiteral("mm²"));
    addBasic(QStringLiteral("打磨严格度"), QStringLiteral("是否进一步剔除无邻居的孤立小马赛克块"),
             QStringLiteral("grinding_strictness_level"));
    addBasic(QStringLiteral("孤立小块面积阈值"), QStringLiteral("严格度 2 下判断“小马赛克块”的面积"),
             QStringLiteral("isolated_mosaic_min_particle_area_mm2"), QStringLiteral("mm²"));
    addBasic(QStringLiteral("邻居判断距离"), QStringLiteral("两块边缘间距在该范围内则不算孤立"),
             QStringLiteral("isolated_mosaic_neighbor_distance_mm"), QStringLiteral("mm"));

    auto *boundary = craftCard(content, left, QStringLiteral("铜板边界与工作区域"),
                               QStringLiteral("只展示算法当前实际采用的处理范围"));
    const auto addBoundary = [boundary, content](const QString &name, const QString &description,
                                                 const QString &key, const QString &unit) {
        boundary->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addBoundary(QStringLiteral("铜板轮廓安全边距"), QStringLiteral("轮廓向内收缩，边缘带不参与检测"),
                QStringLiteral("plate_edge_safety_margin_px"), QStringLiteral("px"));
    addBoundary(QStringLiteral("边界额外排除宽度"), QStringLiteral("抑制铜板四周台阶造成的误检"),
                QStringLiteral("plate_boundary_exclusion_px"), QStringLiteral("px"));
    addBoundary(QStringLiteral("下边缘黑线检测"), QStringLiteral("是否使用拟合黑线校正真实下边界"),
                QStringLiteral("enable_plate_lower_edge_line_detection"), QStringLiteral("开 / 关"));
    addBoundary(QStringLiteral("下边缘搜索范围"), QStringLiteral("从轮廓底部向上搜索黑线的高度"),
                QStringLiteral("plate_lower_edge_search_px"), QStringLiteral("px"));
    addBoundary(QStringLiteral("工作 ROI"), QStringLiteral("算法仅处理该像素矩形内的铜板与粒子"),
                QStringLiteral("region_x1 / region_y1 / region_x2 / region_y2"), QStringLiteral("px"));
    left->addStretch(1);

    auto *bspline = craftCard(content, right, QStringLiteral("B 样条高级信息"),
                              QStringLiteral("基准面拟合、鲁棒重拟合与异常目标过滤参数"));
    const auto addBspline = [bspline, content](const QString &name, const QString &description,
                                               const QString &key, const QString &unit = QString()) {
        bspline->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addBspline(QStringLiteral("X 向控制点数量"), QStringLiteral("控制 X 向板面形变拟合精度，不得小于 4"),
               QStringLiteral("bspline_control_points_x"), QStringLiteral("个"));
    addBspline(QStringLiteral("Y 向控制点数量"), QStringLiteral("控制 Y 向板面形变拟合精度，不得小于 4"),
               QStringLiteral("bspline_control_points_y"), QStringLiteral("个"));
    addBspline(QStringLiteral("曲面平滑系数"), QStringLiteral("越大基准面越平滑，越小越贴近原始板面"),
               QStringLiteral("bspline_smoothing"));
    addBspline(QStringLiteral("拟合抽样步长"), QStringLiteral("深度点抽样间隔，增大可降低计算量"),
               QStringLiteral("bspline_fit_sample_step"), QStringLiteral("px"));
    addBspline(QStringLiteral("鲁棒重拟合次数"), QStringLiteral("逐次减少凸起粒子对基准面的影响"),
               QStringLiteral("bspline_refinement_iterations"), QStringLiteral("次"));
    addBspline(QStringLiteral("低残差样本保留比例"), QStringLiteral("越小越严格排除疑似凸起样本"),
               QStringLiteral("bspline_outlier_rejection_ratio"));
    addBspline(QStringLiteral("初始低高度分位比例"), QStringLiteral("优先选取更可能属于铜板本体的低点"),
               QStringLiteral("bspline_baseline_low_quantile"));
    addBspline(QStringLiteral("异常目标最大宽度"), QStringLiteral("结合长宽比排除过宽长条，0 表示关闭判断"),
               QStringLiteral("bspline_max_particle_width_pixels"), QStringLiteral("px"));
    addBspline(QStringLiteral("过宽长条长宽比"), QStringLiteral("过宽目标被排除时使用的长宽比阈值"),
               QStringLiteral("bspline_max_particle_aspect_ratio"));
    right->addStretch(1);
    return page;
}

} // namespace

CalibrationPlaceholderPage::CalibrationPlaceholderPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("craftPage"));
    setStyleSheet(craftPageStyle());
    Theme::watch(this, [this]() { setStyleSheet(craftPageStyle()); });

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("craftTabs"));
    tabs->addTab(createAlgorithmPage(tabs), QStringLiteral("算法"));
    outer->addWidget(tabs);

    auto *standardPage = new QWidget(tabs);
    standardPage->setObjectName(QStringLiteral("craftContent"));
    auto *standardLayout = new QVBoxLayout(standardPage);
    standardLayout->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(standardPage);
    scroll->setObjectName(QStringLiteral("craftScroll"));
    scroll->viewport()->setObjectName(QStringLiteral("craftViewport"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    standardLayout->addWidget(scroll);
    tabs->insertTab(0, standardPage, QStringLiteral("标定与点云"));
    tabs->setCurrentWidget(standardPage);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("craftContent"));
    content->setMinimumWidth(1100);
    scroll->setWidget(content);
    auto *pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(0, 0, 0, 10);
    pageLayout->setSpacing(14);
    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->setSpacing(14);
    right->setSpacing(14);
    columns->addLayout(left, 42);
    columns->addLayout(right, 58);
    pageLayout->addLayout(columns, 1);

    auto *particle = craftCard(content, left,
                               QStringLiteral("粒子三维点云"),
                               QStringLiteral("等待真实扫描与粒子区域数据 · 当前未采集"));
    auto *preview = new QFrame(content);
    preview->setObjectName(QStringLiteral("craftPreview"));
    preview->setMinimumHeight(240);
    auto *previewLayout = new QVBoxLayout(preview);
    auto *previewText = new QLabel(QStringLiteral("3D 点云预览\n\n未采集"), preview);
    previewText->setObjectName(QStringLiteral("craftPreviewText"));
    previewText->setAlignment(Qt::AlignCenter);
    previewLayout->addWidget(previewText);
    particle->addWidget(preview, 1);
    left->setStretch(0, 1);

    auto *points = craftCard(content, right,
                             QStringLiteral("坐标点对与转换模式"),
                             QStringLiteral("图像坐标与机床坐标的对应关系"));
    auto *pointsEmpty = craftNote(content, QStringLiteral("未采集坐标点对"));
    pointsEmpty->setObjectName(QStringLiteral("craftInfo"));
    points->addWidget(pointsEmpty);
    auto *table = new QTableWidget(0, 8, content);
    table->setObjectName(QStringLiteral("craftTable"));
    table->setHorizontalHeaderLabels({QStringLiteral("#"), QStringLiteral("Img X"),
                                      QStringLiteral("Img Y"), QStringLiteral("Img Z"),
                                      QStringLiteral("Rob X"), QStringLiteral("Rob Y"),
                                      QStringLiteral("Rob Z"), QStringLiteral("残差")});
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setFixedHeight(38);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setAlternatingRowColors(true);
    table->setMinimumHeight(200);
    points->addWidget(table, 1);
    points->addWidget(craftNote(content, QStringLiteral("坐标转换模式 · 未设置")));
    craftSegments(content, points,
                  {QStringLiteral("刚性"), QStringLiteral("等比缩放"),
                   QStringLiteral("非等比缩放"), QStringLiteral("仿射")}, -1);
    right->setStretch(0, 1);

    auto *matrixCard = craftCard(content, pageLayout,
                                 QStringLiteral("坐标转换矩阵"),
                                 QStringLiteral("未标定 · 尚无真实转换结果"));
    auto *matrix = new QFrame(content);
    matrix->setObjectName(QStringLiteral("craftMatrix"));
    auto *grid = new QGridLayout(matrix);
    grid->setContentsMargins(12, 12, 12, 12);
    grid->setSpacing(7);
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            auto *label = new QLabel(QStringLiteral("—"), matrix);
            label->setObjectName(QStringLiteral("craftMatrixCell"));
            label->setAlignment(Qt::AlignCenter);
            label->setMinimumHeight(34);
            grid->addWidget(label, r, c);
        }
    }
    matrixCard->addWidget(matrix);
    pageLayout->addWidget(craftNote(content, QStringLiteral("数据状态 · 未接入真实采集与标定数据时仅显示空状态。")));
}
