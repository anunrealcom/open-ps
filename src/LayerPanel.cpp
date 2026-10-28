#include "LayerPanel.h"
#include "Document.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QListWidget>
#include <QListWidgetItem>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>

namespace {
QIcon layerThumb(const QImage &img)
{
    const QSize box(44, 34);
    QPixmap pm(box);
    pm.fill(QColor("#dfe3e8"));
    if (!img.isNull()) {
        QImage scaled = img.scaled(box, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPainter p(&pm);
        p.drawImage((box.width() - scaled.width()) / 2,
                    (box.height() - scaled.height()) / 2, scaled);
        p.end();
    }
    return QIcon(pm);
}
}

LayerPanel::LayerPanel(QWidget *parent)
    : QWidget(parent)
{
    setMinimumWidth(220);

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(44, 34));
    connect(m_list, &QListWidget::currentRowChanged, this, &LayerPanel::onRowChanged);
    connect(m_list, &QListWidget::itemChanged, this, &LayerPanel::onItemChanged);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &LayerPanel::onItemDoubleClicked);

    // 混合模式
    auto *blendRow = new QHBoxLayout;
    blendRow->addWidget(new QLabel(tr("混合"), this));
    m_blend = new QComboBox(this);
    m_blend->addItem(tr("正常"));
    m_blend->addItem(tr("正片叠底"));
    m_blend->addItem(tr("滤色"));
    m_blend->setCurrentIndex(0);
    blendRow->addWidget(m_blend, 1);
    connect(m_blend, QOverload<int>::of(&QComboBox::activated), this, &LayerPanel::onBlendChanged);

    // 透明度
    auto *opRow = new QHBoxLayout;
    opRow->addWidget(new QLabel(tr("不透明"), this));
    m_opacity = new QSlider(Qt::Horizontal, this);
    m_opacity->setRange(0, 100);
    m_opacity->setValue(100);
    m_opacityVal = new QLabel("100%", this);
    m_opacityVal->setMinimumWidth(40);
    m_opacityVal->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    opRow->addWidget(m_opacity, 1);
    opRow->addWidget(m_opacityVal);
    connect(m_opacity, &QSlider::valueChanged, this, &LayerPanel::onOpacityChanged);

    // 按钮（图标 + 悬停提示，避免文字被挤窄）
    auto *btnRow = new QHBoxLayout;
    btnRow->setSpacing(4);
    auto makeBtn = [this](const QString &iconName, const QString &tip) {
        auto *b = new QPushButton(this);
        b->setIcon(IconFactory::icon(iconName, Theme::accent()));
        b->setIconSize(QSize(18, 18));
        b->setFixedSize(34, 30);
        b->setToolTip(tip);
        b->setStyleSheet("QPushButton{border:1px solid #E6E9EE;border-radius:6px;background:#fff;}"
                         "QPushButton:hover{background:#E9F6FD;border-color:#12B7F5;}"
                         "QPushButton:pressed{background:#D6EEFA;}");
        return b;
    };
    m_addBtn = makeBtn("new", tr("新建图层"));
    m_dupBtn = makeBtn("copy", tr("复制图层"));
    m_delBtn = makeBtn("delete", tr("删除图层"));
    m_upBtn = makeBtn("arrow-up", tr("上移图层"));
    m_downBtn = makeBtn("arrow-down", tr("下移图层"));
    for (QPushButton *b : { m_addBtn, m_dupBtn, m_delBtn, m_upBtn, m_downBtn })
        btnRow->addWidget(b);
    btnRow->addStretch(1);

    connect(m_addBtn, &QPushButton::clicked, this, [this] { emit requestAddLayer(); });
    connect(m_delBtn, &QPushButton::clicked, this, [this] {
        int i = selectedLayerIndex(); if (i >= 0) emit requestRemoveLayer(i);
    });
    connect(m_dupBtn, &QPushButton::clicked, this, [this] {
        int i = selectedLayerIndex(); if (i >= 0) emit requestDuplicateLayer(i);
    });
    connect(m_upBtn, &QPushButton::clicked, this, [this] {
        int i = selectedLayerIndex();
        if (i >= 0 && i < m_list->count() - 1) emit requestMoveLayer(i, i + 1);
    });
    connect(m_downBtn, &QPushButton::clicked, this, [this] {
        int i = selectedLayerIndex();
        if (i > 0) emit requestMoveLayer(i, i - 1);
    });

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->addWidget(new QLabel(tr("图层"), this));
    lay->addWidget(m_list, 1);
    lay->addLayout(blendRow);
    lay->addLayout(opRow);
    lay->addLayout(btnRow);
}

void LayerPanel::refresh(const Document &doc)
{
    m_updating = true;
    const int count = doc.layerCount();
    m_list->clear();
    for (int row = 0; row < count; ++row) {
        const int idx = rowToLayer(row, count);
        const Layer &l = doc.layerAt(idx);
        auto *item = new QListWidgetItem(layerThumb(l.image), l.name);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsEditable);
        item->setCheckState(l.visible ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole + 1, l.visible);
        item->setData(Qt::UserRole + 2, l.name);
        if (idx == doc.activeIndex())
            item->setForeground(QColor("#12B7F5"));
        m_list->addItem(item);
    }
    const int activeRow = count - 1 - doc.activeIndex();
    if (activeRow >= 0 && activeRow < count)
        m_list->setCurrentRow(activeRow);

    if (!doc.isNull()) {
        const int op = doc.activeLayer() ? doc.activeLayer()->opacity : 255;
        QSignalBlocker b(m_opacity);
        m_opacity->setValue(int(op * 100 / 255));
        m_opacityVal->setText(QString::number(m_opacity->value()) + "%");

        if (m_blend && doc.activeLayer()) {
            QSignalBlocker bb(m_blend);
            m_blend->setCurrentIndex(int(doc.activeLayer()->blend));
        }
    }
    m_updating = false;
}

int LayerPanel::selectedLayerIndex() const
{
    const int row = m_list->currentRow();
    if (row < 0)
        return -1;
    return rowToLayer(row, m_list->count());
}

void LayerPanel::onRowChanged(int row)
{
    if (m_updating || row < 0)
        return;
    const int idx = rowToLayer(row, m_list->count());
    emit requestSetActive(idx);
}

void LayerPanel::onItemChanged(QListWidgetItem *item)
{
    if (m_updating || !item)
        return;
    const int row = m_list->row(item);
    const int idx = rowToLayer(row, m_list->count());
    const bool vis = (item->checkState() == Qt::Checked);
    if (vis != item->data(Qt::UserRole + 1).toBool()) {
        item->setData(Qt::UserRole + 1, vis);
        emit requestSetVisible(idx, vis);
    }
    const QString name = item->text();
    if (name != item->data(Qt::UserRole + 2).toString()) {
        item->setData(Qt::UserRole + 2, name);
        emit requestRename(idx, name);
    }
}

void LayerPanel::onItemDoubleClicked(QListWidgetItem *item)
{
    // 双击进入编辑（ItemIsEditable 已开启），此处无需额外处理
    Q_UNUSED(item);
}

void LayerPanel::onOpacityChanged(int value)
{
    if (m_opacityVal)
        m_opacityVal->setText(QString::number(value) + "%");
    if (m_updating)
        return;
    const int idx = selectedLayerIndex();
    if (idx >= 0)
        emit requestSetOpacity(idx, int(value * 255 / 100));
}

void LayerPanel::onBlendChanged(int index)
{
    if (m_updating)
        return;
    const int idx = selectedLayerIndex();
    if (idx >= 0)
        emit requestSetBlend(idx, static_cast<Blend>(index));
}
