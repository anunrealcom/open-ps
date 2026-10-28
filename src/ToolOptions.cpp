#include "ToolOptions.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QColorDialog>

ToolOptions::ToolOptions(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(200);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(8);

    auto *title = new QLabel(tr("画笔选项"), this);
    title->setStyleSheet("color:#8A9099; font-weight:bold;");
    lay->addWidget(title);

    // ---- 前景色 ----
    auto *colorRow = new QHBoxLayout;
    m_swatch = new QPushButton(this);
    m_swatch->setFixedSize(64, 30);
    m_swatch->setToolTip(tr("点击选择前景色"));
    connect(m_swatch, &QPushButton::clicked, this, [this] {
        const QColor c = QColorDialog::getColor(m_color, this, tr("选择前景色"));
        if (c.isValid()) { setColor(c); emit colorChosen(c); }
    });
    auto *customBtn = new QPushButton(tr("自定义…"), this);
    connect(customBtn, &QPushButton::clicked, m_swatch, &QPushButton::click);
    colorRow->addWidget(m_swatch);
    colorRow->addWidget(customBtn);
    colorRow->addStretch(1);
    lay->addLayout(colorRow);

    // ---- 色板 ----
    lay->addWidget(new QLabel(tr("常用色"), this));
    static const char *palette[] = {
        "#000000", "#7f7f7f", "#ffffff", "#ff4d4d", "#ff9f40", "#ffd60a",
        "#4ade80", "#12B7F5", "#3b5bdb", "#9b5de5", "#ff6fb5", "#8d5524",
        "#00b8a9", "#c0c5ce"
    };
    auto *grid = new QGridLayout;
    grid->setSpacing(4);
    const int cols = 7;
    for (int i = 0; i < int(sizeof(palette) / sizeof(palette[0])); ++i) {
        const QColor col(QString::fromUtf8(palette[i]));
        auto *b = new QPushButton(this);
        b->setFixedSize(24, 24);
        b->setCursor(Qt::PointingHandCursor);
        b->setStyleSheet(QString(
            "QPushButton{ background:%1; border:1px solid #c9ced6; border-radius:5px; }"
            "QPushButton:hover{ border:2px solid #12B7F5; }").arg(col.name()));
        connect(b, &QPushButton::clicked, this, [this, col] {
            setColor(col);
            emit colorChosen(col);
        });
        grid->addWidget(b, i / cols, i % cols);
    }
    lay->addLayout(grid);

    // ---- 大小 ----
    lay->addWidget(new QLabel(tr("画笔 / 橡皮大小"), this));
    auto *sizeRow = new QHBoxLayout;
    m_size = new QSlider(Qt::Horizontal, this);
    m_size->setRange(1, 120);
    m_size->setValue(4);
    m_sizeVal = new QLabel("4", this);
    m_sizeVal->setMinimumWidth(28);
    m_sizeVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    connect(m_size, &QSlider::valueChanged, this, [this](int v) {
        m_sizeVal->setText(QString::number(v));
        emit sizeChanged(v);
    });
    sizeRow->addWidget(m_size, 1);
    sizeRow->addWidget(m_sizeVal);
    lay->addLayout(sizeRow);

    lay->addSpacing(4);

    // ---- 导入图片 ----
    auto *importBtn = new QPushButton(IconFactory::icon("image", Theme::accent()),
                                      tr(" 导入图片为图层"), this);
    importBtn->setIconSize(QSize(18, 18));
    connect(importBtn, &QPushButton::clicked, this, &ToolOptions::importRequested);
    lay->addWidget(importBtn);

    lay->addStretch(1);

    rebuildSwatch();
}

void ToolOptions::setColor(const QColor &c)
{
    m_color = c;
    rebuildSwatch();
}

void ToolOptions::setSize(int s)
{
    if (s < 0) return;
    QSignalBlocker b(m_size);
    m_size->setValue(qBound(1, s, 120));
    m_sizeVal->setText(QString::number(m_size->value()));
}

void ToolOptions::rebuildSwatch()
{
    if (!m_swatch) return;
    m_swatch->setStyleSheet(QString(
        "QPushButton{ background:%1; border:1px solid #c9ced6; border-radius:6px; }"
        "QPushButton:hover{ border:2px solid #12B7F5; }").arg(m_color.name()));
    m_swatch->setText(QString());
    m_swatch->setToolTip(tr("前景色 %1（点击修改）").arg(m_color.name().toUpper()));
}
