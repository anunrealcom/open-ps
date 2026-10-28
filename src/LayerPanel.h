#pragma once

#include <QWidget>
#include <QVector>
#include "Document.h"

class QListWidget;
class QListWidgetItem;
class QSlider;
class QLabel;
class QPushButton;
class QComboBox;
class Document;

// ============================================================================
// LayerPanel: 图层面板。列表顶部 = 最上层。支持新建/删除/复制/上移/下移/
// 显隐/透明度/重命名/选中，操作通过信号请求 MainWindow 执行（便于撤销）。
// ============================================================================
class LayerPanel : public QWidget
{
    Q_OBJECT
public:
    explicit LayerPanel(QWidget *parent = nullptr);

    void refresh(const Document &doc);   // 依据文档重建列表（不触发信号）

signals:
    void requestAddLayer();
    void requestRemoveLayer(int index);
    void requestDuplicateLayer(int index);
    void requestMoveLayer(int from, int to);
    void requestSetActive(int index);
    void requestSetVisible(int index, bool visible);
    void requestSetOpacity(int index, int opacity);
    void requestSetBlend(int index, Blend blend);
    void requestRename(int index, const QString &name);

private slots:
    void onRowChanged(int row);
    void onItemChanged(QListWidgetItem *item);
    void onItemDoubleClicked(QListWidgetItem *item);
    void onOpacityChanged(int value);
    void onBlendChanged(int index);

private:
    static int rowToLayer(int row, int count) { return count - 1 - row; }
    int selectedLayerIndex() const;

    QListWidget *m_list = nullptr;
    QComboBox *m_blend = nullptr;
    QSlider *m_opacity = nullptr;
    QLabel *m_opacityVal = nullptr;
    QPushButton *m_addBtn = nullptr;
    QPushButton *m_delBtn = nullptr;
    QPushButton *m_dupBtn = nullptr;
    QPushButton *m_upBtn = nullptr;
    QPushButton *m_downBtn = nullptr;

    bool m_updating = false;   // 抑制程序刷新时触发的信号
};
