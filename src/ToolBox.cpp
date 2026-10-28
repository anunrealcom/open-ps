#include "ToolBox.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QToolButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QLabel>

ToolBox::ToolBox(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(96);
    setMaximumWidth(120);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 8, 6, 8);
    lay->setSpacing(2);
    auto *title = new QLabel(tr("工具箱"), this);
    title->setStyleSheet("color:#8A9099; font-weight:bold; padding:2px 4px;");
    lay->addWidget(title);

    m_group = new QButtonGroup(this);
    m_group->setExclusive(true);

    struct Item { const char *label; const char *icon; ImageCanvas::Tool tool; };
    const Item items[] = {
        {"移动",   "pan",     ImageCanvas::Tool::Pan},
        {"画笔",   "brush",   ImageCanvas::Tool::Brush},
        {"铅笔",   "pencil",  ImageCanvas::Tool::Pencil},
        {"橡皮",   "eraser",  ImageCanvas::Tool::Eraser},
        {"直线",   "line",    ImageCanvas::Tool::Line},
        {"矩形",   "rect",    ImageCanvas::Tool::Rect},
        {"椭圆",   "ellipse", ImageCanvas::Tool::Ellipse},
        {"油漆桶", "fill",    ImageCanvas::Tool::Fill},
        {"取色器", "picker",  ImageCanvas::Tool::Picker},
        {"文字",   "text",    ImageCanvas::Tool::TextTool},
        {"裁剪",   "crop",    ImageCanvas::Tool::Crop},
        {"遮罩框", "mask",    ImageCanvas::Tool::MaskRect},
        {"遮罩笔", "mask",    ImageCanvas::Tool::MaskBrush},
    };

    for (const Item &it : items) {
        auto *btn = new QToolButton(this);
        btn->setText(tr(it.label));
        btn->setIcon(IconFactory::icon(it.icon, Theme::accent()));
        btn->setIconSize(QSize(18, 18));
        btn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        btn->setCheckable(true);
        btn->setAutoRaise(true);
        btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        btn->setStyleSheet(
            "QToolButton{ text-align:left; padding:6px 8px; border-radius:8px; }"
            "QToolButton:hover{ background:#E9F6FD; }"
            "QToolButton:checked{ background:#12B7F5; color:white; }");
        m_group->addButton(btn, int(it.tool));
        lay->addWidget(btn);
    }

    connect(m_group, &QButtonGroup::idClicked, this, [this](int id) {
        emit toolChosen(static_cast<ImageCanvas::Tool>(id));
    });

    // 默认选中画笔
    if (auto *b = m_group->button(int(ImageCanvas::Tool::Brush)))
        b->setChecked(true);

    lay->addStretch(1);
}

void ToolBox::setTool(ImageCanvas::Tool tool)
{
    if (auto *b = m_group->button(int(tool))) {
        QSignalBlocker blk(m_group);
        b->setChecked(true);
    }
}
