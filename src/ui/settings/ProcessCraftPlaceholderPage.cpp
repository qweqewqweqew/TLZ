#include "ui/settings/ProcessCraftPlaceholderPage.h"

#include "app/Theme.h"
#include "ui/settings/CraftPlaceholder.h"
#include "ui/settings/ProgramParamDisplay.h"

#include <QAbstractSpinBox>
#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

void setUnset(QDoubleSpinBox *spin)
{
    spin->setMinimum(spin->minimum() - spin->singleStep());
    spin->setSpecialValueText(QStringLiteral("未设置"));
    spin->setValue(spin->minimum());
}

QWidget *speedRow(QWidget *parent, const QString &axis)
{
    auto *row = new QWidget(parent);
    row->setMinimumHeight(44);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto *name = new QLabel(axis, row);
    name->setObjectName(QStringLiteral("craftLabel"));
    name->setMinimumWidth(28);
    auto *rapidSpin = craftDouble(parent, 0.0, 0, 0.0, 100000.0, 10.0);
    auto *cuttingSpin = craftDouble(parent, 0.0, 0, 0.0, 100000.0, 10.0);
    setUnset(rapidSpin);
    setUnset(cuttingSpin);
    rapidSpin->setObjectName(QStringLiteral("craftRapidValue"));
    cuttingSpin->setObjectName(QStringLiteral("craftCuttingValue"));
    layout->addWidget(name);
    layout->addWidget(rapidSpin, 1);
    layout->addWidget(cuttingSpin, 1);
    return row;
}

QWidget *coefficientRow(QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);
    for (int i = 0; i < 4; ++i) {
        auto *box = new QWidget(row);
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(4);
        boxLayout->addWidget(craftNote(box, QStringLiteral("K%1").arg(i + 1)));
        auto *coefficient = craftDouble(box, 0.0, 2, 0.0, 10.0, 0.01);
        setUnset(coefficient);
        boxLayout->addWidget(coefficient);
        layout->addWidget(box, 1);
    }
    return row;
}

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

    auto *tool = craftCard(content, left, QStringLiteral("刀具、步距与分层"),
                           QStringLiteral("算法当前使用的覆盖几何与多层加工规则"));
    const auto addTool = [tool, content](const QString &name, const QString &description,
                                         const QString &key, const QString &unit = QString()) {
        tool->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addTool(QStringLiteral("刀具直径"), QStringLiteral("计算刀盘覆盖范围和默认扫描步距"),
            QStringLiteral("tool_size"), QStringLiteral("mm"));
    addTool(QStringLiteral("刀盘中心无效区"), QStringLiteral("中心无有效磨削能力区域的直径"),
            QStringLiteral("tool_center_dead_zone_diameter_mm"), QStringLiteral("mm"));
    addTool(QStringLiteral("Y 向扫描步距"), QStringLiteral("相邻扫描线间距；非正值时按刀具直径一半自动计算"),
            QStringLiteral("stepover"), QStringLiteral("mm"));
    addTool(QStringLiteral("单层最大下刀深度"), QStringLiteral("较高目标按照该深度拆分为多层加工"),
            QStringLiteral("single_down"), QStringLiteral("mm"));
    addTool(QStringLiteral("多层加工策略"), QStringLiteral("每条扫描段完成全部层数，或按层遍历全部扫描段"),
            QStringLiteral("grinding_pass_strategy"));

    auto *speed = craftCard(content, left, QStringLiteral("算法输出速度"),
                            QStringLiteral("随轨迹输出给执行端的加工与空行程速度"));
    const auto addSpeed = [speed, content](const QString &name, const QString &description,
                                           const QString &key) {
        speed->addWidget(craftReadOnlyRow(content, name, description, key, QStringLiteral("速度单位以算法配置为准")));
    };
    addSpeed(QStringLiteral("姿态速度 R"), QStringLiteral("机器人旋转或姿态相关速度"), QStringLiteral("r_speed"));
    addSpeed(QStringLiteral("加工速度 X"), QStringLiteral("机器人 X 方向加工运动速度"), QStringLiteral("x_speed"));
    addSpeed(QStringLiteral("加工速度 Y"), QStringLiteral("机器人 Y 方向加工运动速度"), QStringLiteral("y_speed"));
    addSpeed(QStringLiteral("加工速度 Z"), QStringLiteral("机器人 Z 方向加工运动速度"), QStringLiteral("z_speed"));
    addSpeed(QStringLiteral("空行程速度"), QStringLiteral("非加工、抬刀等运动使用的速度"), QStringLiteral("no_load_speed"));

    auto *path = craftCard(content, right, QStringLiteral("路径覆盖与边缘处理"),
                           QStringLiteral("减少无效行程、抬刀与目标边缘台阶"));
    const auto addPath = [path, content](const QString &name, const QString &description,
                                         const QString &key, const QString &unit = QString()) {
        path->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addPath(QStringLiteral("目标掩膜扫描"), QStringLiteral("是否仅根据有目标的区域生成扫描行"),
            QStringLiteral("enable_target_mask_scan"), QStringLiteral("开 / 关"));
    addPath(QStringLiteral("目标段合并间隙"), QStringLiteral("小于该间隙的相邻目标段合并，减少分段"),
            QStringLiteral("target_mask_run_merge_gap_mm"), QStringLiteral("mm"));
    addPath(QStringLiteral("连续 Y 向过渡"), QStringLiteral("相邻扫描行之间减少回退或抬刀"),
            QStringLiteral("enableContinuousYTransition"), QStringLiteral("开 / 关"));
    addPath(QStringLiteral("边缘羽化"), QStringLiteral("降低目标区域边缘去除深度，减小台阶"),
            QStringLiteral("enable_edge_feathering"), QStringLiteral("开 / 关"));
    addPath(QStringLiteral("羽化带宽度"), QStringLiteral("从目标区域边界向内的羽化范围"),
            QStringLiteral("edge_feather_width_mm"), QStringLiteral("mm"));
    addPath(QStringLiteral("羽化深度级距"), QStringLiteral("为 0 时自动取单层下刀深度的十分之一"),
            QStringLiteral("edge_feather_depth_step_mm"), QStringLiteral("mm"));

    auto *safety = craftCard(content, right, QStringLiteral("安全高度与坐标约束"),
                             QStringLiteral("算法输出轨迹前使用的安全净空与位置限制"));
    const auto addSafety = [safety, content](const QString &name, const QString &description,
                                             const QString &key, const QString &unit = QString()) {
        safety->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addSafety(QStringLiteral("安全抬刀高度"), QStringLiteral("规划坐标系中的安全高度或安全余量"),
              QStringLiteral("safetyZ"), QStringLiteral("mm"));
    addSafety(QStringLiteral("快速移动安全净空"), QStringLiteral("相对最高待磨粒子额外保留的高度"),
              QStringLiteral("rapid_clearance_margin_mm"), QStringLiteral("mm"));
    addSafety(QStringLiteral("Z 向加工偏置"), QStringLiteral("加工深度或坐标换算使用的 Z 向补偿"),
              QStringLiteral("offset"), QStringLiteral("mm"));
    addSafety(QStringLiteral("机器人等待位置"), QStringLiteral("等待或待机使用的机床坐标"),
              QStringLiteral("robot_wait_x / robot_wait_y / robot_wait_z"), QStringLiteral("mm"));
    addSafety(QStringLiteral("机器人 Y 下限过滤"), QStringLiteral("是否丢弃任一端点低于 Y 下限的整条路径"),
              QStringLiteral("enable_min_robot_y_path_filter"), QStringLiteral("开 / 关"));
    addSafety(QStringLiteral("路径最小 Y 坐标"), QStringLiteral("启用下限过滤后允许发布的最小机器人 Y 坐标"),
              QStringLiteral("min_robot_path_y"), QStringLiteral("mm"));

    auto *optimization = craftCard(content, left, QStringLiteral("区域访问顺序优化"),
                                   QStringLiteral("只显示优化策略，不提供界面调参"));
    const auto addOptimization = [optimization, content](const QString &name, const QString &description,
                                                         const QString &key, const QString &unit = QString()) {
        optimization->addWidget(craftReadOnlyRow(content, name, description, key, unit));
    };
    addOptimization(QStringLiteral("最近区域优先"), QStringLiteral("是否优先加工距离当前位置最近的区域"),
                    QStringLiteral("force_nearest_first_region"), QStringLiteral("开 / 关"));
    addOptimization(QStringLiteral("遗传路径优化"), QStringLiteral("是否优化区域访问顺序"),
                    QStringLiteral("enable_genetic_path_optimization"), QStringLiteral("开 / 关"));
    addOptimization(QStringLiteral("遗传种群大小"), QStringLiteral("增大通常提高搜索质量并增加耗时"),
                    QStringLiteral("genetic_population_size"), QStringLiteral("个"));
    addOptimization(QStringLiteral("遗传迭代代数"), QStringLiteral("增大通常提高搜索质量并增加耗时"),
                    QStringLiteral("genetic_generations"), QStringLiteral("代"));
    left->addStretch(1);
    right->addStretch(1);
    return page;
}

} // namespace

ProcessCraftPlaceholderPage::ProcessCraftPlaceholderPage(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("craftPage"));
    setStyleSheet(craftPageStyle());
    Theme::watch(this, [this]() {
        setStyleSheet(craftPageStyle());
        update();
    });

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName(QStringLiteral("craftTabs"));

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

    tabs->addTab(standardPage, QStringLiteral("工艺规划"));
    tabs->addTab(createAlgorithmPage(tabs), QStringLiteral("算法"));
    tabs->setCurrentWidget(standardPage);
    attachProgramParamDisplay(this);
    outer->addWidget(tabs);

    auto *content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("craftContent"));
    content->setMinimumWidth(1100);
    scroll->setWidget(content);
    auto *pageLayout = new QVBoxLayout(content);
    pageLayout->setContentsMargins(0, 0, 0, 10);
    pageLayout->setSpacing(12);
    auto *state = craftNote(content,
        QStringLiteral("工艺规划 · 只读｜尚未读取 m_sJggyPara 或设备实时配置。页面数字仅为需求提供的代码默认值，不是现场当前值；无来源项显示“未设置”。"));
    state->setObjectName(QStringLiteral("craftWarning"));
    pageLayout->addWidget(state);

    auto *columns = new QHBoxLayout();
    columns->setSpacing(14);
    auto *left = new QVBoxLayout();
    auto *right = new QVBoxLayout();
    left->setSpacing(14);
    right->setSpacing(14);
    columns->addLayout(left, 1);
    columns->addLayout(right, 1);
    pageLayout->addLayout(columns, 1);

    auto *fixed = craftCard(content, left,
                            QStringLiteral("固定工艺参数与速度规划"),
                            QStringLiteral("无来源的主轴、速度和系数保持未设置 · 速度单位 mm/min"));
    auto *spindle = craftDouble(content, 0.0, 0, 0.0, 100000.0, 100.0);
    setUnset(spindle);
    fixed->addWidget(craftRow(content, QStringLiteral("主轴转速"), spindle, QStringLiteral("rpm")));
    auto *diameter = craftDouble(content, 80.0, 1, 1.0, 500.0, 1.0);
    fixed->addWidget(craftRow(content, QStringLiteral("刀具直径"), diameter, QStringLiteral("mm")));
    auto *legend = new QHBoxLayout();
    legend->addStretch(1);
    auto *rapidLegend = craftNote(content, QStringLiteral("空跑最大速度"));
    rapidLegend->setObjectName(QStringLiteral("craftRapidLabel"));
    auto *cutLegend = craftNote(content, QStringLiteral("加工最大速度"));
    cutLegend->setObjectName(QStringLiteral("craftCuttingLabel"));
    legend->addWidget(rapidLegend);
    legend->addSpacing(24);
    legend->addWidget(cutLegend);
    fixed->addLayout(legend);
    fixed->addWidget(speedRow(content, QStringLiteral("X")));
    fixed->addWidget(speedRow(content, QStringLiteral("Y")));
    fixed->addWidget(speedRow(content, QStringLiteral("Z")));
    fixed->addWidget(craftNote(content, QStringLiteral("XY 速度规划模式 · 未设置")));
    craftSegments(content, fixed,
                  {QStringLiteral("恒定"), QStringLiteral("轴比例"), QStringLiteral("二次约束")}, -1);
    fixed->addWidget(coefficientRow(content));
    fixed->addWidget(craftNote(content, QStringLiteral("Z 速度规划模式 · 未设置")));
    craftSegments(content, fixed,
                  {QStringLiteral("恒定"), QStringLiteral("轴比例"), QStringLiteral("二次约束")}, -1);
    fixed->addWidget(coefficientRow(content));

    auto *feed = craftCard(content, left,
                           QStringLiteral("Y 轴进给与加工排序"),
                           QStringLiteral("固定进给或按刀盘比例进给，并设置边界安全距离"));
    auto *yGroup = craftSegments(content, feed,
                                 {QStringLiteral("固定进给"), QStringLiteral("按刀盘比例")}, 0);
    auto *fixedFeed = craftDouble(content, 70.0, 1, 0.0, 500.0, 1.0);
    auto *fixedFeedHost = craftRow(content, QStringLiteral("固定 Y 向进给"), fixedFeed, QStringLiteral("mm"));
    feed->addWidget(fixedFeedHost);
    auto *ratio = craftDouble(content, 0.80, 2, 0.0, 2.0, 0.01);
    auto *ratioHost = craftRow(content, QStringLiteral("刀盘比例系数 K"), ratio, QString());
    feed->addWidget(ratioHost);
    auto *ratioResult = craftNote(content, QString());
    feed->addWidget(ratioResult);
    feed->addWidget(craftRow(content, QStringLiteral("气缸侧安全距离"),
                             craftDouble(content, 60.0, 1, 0.0, 500.0, 1.0), QStringLiteral("mm")));
    auto *safeDefinition = craftNote(content, QStringLiteral("安全距离指加工中心到气缸的距离，尚未计入刀盘直径。"));
    safeDefinition->setObjectName(QStringLiteral("craftWarning"));
    feed->addWidget(safeDefinition);
    auto *sort = new QComboBox(content);
    sort->addItems({QStringLiteral("从上到下"), QStringLiteral("从下到上"),
                    QStringLiteral("从左到右"), QStringLiteral("从右到左"),
                    QStringLiteral("局部最近"), QStringLiteral("全局最短")});
    sort->setCurrentIndex(4);
    feed->addWidget(craftRow(content, QStringLiteral("粒子加工排序"), sort, QString()));
    const auto updateFeed = [yGroup, diameter, fixedFeed, fixedFeedHost,
                             ratio, ratioHost, ratioResult]() {
        const bool useRatio = yGroup->checkedId() == 1;
        fixedFeedHost->setEnabled(!useRatio);
        ratioHost->setEnabled(useRatio);
        const double edge = ratio->value() * diameter->value();
        ratioResult->setText(useRatio
                                 ? QStringLiteral("刀盘方向进给 = %1 × Φ%2 = %3 mm")
                                       .arg(ratio->value(), 0, 'f', 2)
                                       .arg(diameter->value(), 0, 'f', 1)
                                       .arg(edge, 0, 'f', 1)
                                 : QStringLiteral("固定 Y 向进给 = %1 mm，刀盘比例系数不生效。")
                                       .arg(fixedFeed->value(), 0, 'f', 1));
    };
    updateFeed();
    connect(yGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, updateFeed);
    connect(diameter, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateFeed);
    connect(fixedFeed, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateFeed);
    connect(ratio, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, updateFeed);
    left->addStretch(1);

    auto *height = craftCard(content, right,
                             QStringLiteral("Z 轴高度与路径进给"),
                             QStringLiteral("最高点、最低点、抬刀高度与 Z 向进给参数"));
    height->addWidget(craftRow(content, QStringLiteral("粒子最高点上抬"),
                               craftDouble(content, 2.0, 2, -20.0, 50.0, 0.1), QStringLiteral("mm")));
    height->addWidget(craftRow(content, QStringLiteral("铜板上平面上抬"),
                               craftDouble(content, 0.0, 2, -20.0, 50.0, 0.1), QStringLiteral("mm")));
    height->addWidget(craftRow(content, QStringLiteral("完工抬刀高度"),
                               craftDouble(content, 5.0, 2, 0.0, 100.0, 0.1), QStringLiteral("mm")));
    auto *zGroup = craftSegments(content, height,
                                 {QStringLiteral("等距进给"), QStringLiteral("线性进给")}, 1);
    auto *fixedZStep = craftDouble(content, 2.0, 2, 0.0, 100.0, 0.1);
    auto *fixedZHost = craftRow(content, QStringLiteral("固定 Z 向进给"), fixedZStep, QStringLiteral("mm"));
    height->addWidget(fixedZHost);
    auto *zFactors = new QWidget(content);
    auto *zFactorLayout = new QHBoxLayout(zFactors);
    zFactorLayout->setContentsMargins(0, 0, 0, 0);
    zFactorLayout->setSpacing(6);
    const QList<QPair<QString, double>> factors = {
        {QStringLiteral("K"), 0.50},
        {QStringLiteral("B"), 1.50},
        {QStringLiteral("Min"), 3.00},
        {QStringLiteral("Max"), 10.00},
    };
    for (const auto &factor : factors) {
        auto *box = new QWidget(zFactors);
        auto *boxLayout = new QVBoxLayout(box);
        boxLayout->setContentsMargins(0, 0, 0, 0);
        boxLayout->setSpacing(4);
        boxLayout->addWidget(craftNote(box, factor.first));
        boxLayout->addWidget(craftDouble(box, factor.second, 2, -100.0, 100.0, 0.1));
        zFactorLayout->addWidget(box, 1);
    }
    height->addWidget(zFactors);
    const auto updateZMode = [zGroup, fixedZHost, zFactors]() {
        const bool linear = zGroup->checkedId() == 1;
        fixedZHost->setEnabled(!linear);
        zFactors->setEnabled(linear);
    };
    updateZMode();
    connect(zGroup, QOverload<int>::of(&QButtonGroup::idClicked), this, updateZMode);

    auto *offsets = craftCard(content, right,
                              QStringLiteral("路径偏移与机床补偿"),
                              QStringLiteral("下刀位置、路径端点与整体坐标偏移 · Z 精调范围 ±2 mm"));
    offsets->addWidget(craftRow(content, QStringLiteral("下刀位置偏移"),
                                craftDouble(content, 50.0, 2, -500.0, 500.0, 0.1), QStringLiteral("mm")));
    offsets->addWidget(craftRow(content, QStringLiteral("路径段左偏移"),
                                craftDouble(content, 0.0, 2, -500.0, 500.0, 0.1), QStringLiteral("mm")));
    offsets->addWidget(craftRow(content, QStringLiteral("路径段右偏移"),
                                craftDouble(content, 0.0, 2, -500.0, 500.0, 0.1), QStringLiteral("mm")));
    offsets->addWidget(craftRow(content, QStringLiteral("路径点整体偏移 X"),
                                craftDouble(content, 0.0, 2, -500.0, 500.0, 0.1), QStringLiteral("mm")));
    offsets->addWidget(craftRow(content, QStringLiteral("路径点整体偏移 Y"),
                                craftDouble(content, 0.0, 2, -500.0, 500.0, 0.1), QStringLiteral("mm")));
    offsets->addWidget(craftRow(content, QStringLiteral("路径点整体偏移 Z"),
                                craftDouble(content, 0.0, 2, -2.0, 2.0, 0.1), QStringLiteral("mm")));
    right->addWidget(craftNote(content, QStringLiteral("数据状态 · 只读展示；未接入实时读取、保存与下发。")));
    right->addStretch(1);

    const auto editors = content->findChildren<QAbstractSpinBox *>();
    for (auto *editor : editors) {
        editor->setReadOnly(true);
        editor->setButtonSymbols(QAbstractSpinBox::NoButtons);
        editor->setFocusPolicy(Qt::NoFocus);
        editor->setCursor(Qt::ArrowCursor);
        editor->setToolTip(QStringLiteral("只读：尚未读取实时工艺配置，不能在此修改"));
    }
    const auto combos = content->findChildren<QComboBox *>();
    for (auto *combo : combos) {
        combo->setEnabled(false);
        combo->setToolTip(QStringLiteral("只读：尚未读取实时工艺配置，不能在此修改"));
    }
    const auto buttons = content->findChildren<QPushButton *>();
    for (auto *button : buttons) {
        button->setEnabled(false);
        button->setToolTip(QStringLiteral("只读：尚未读取实时工艺配置，不能在此修改"));
    }
}
