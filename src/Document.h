#pragma once

#include <QImage>
#include <QString>
#include <QVector>
#include <QSize>
#include <QColor>
#include <QRect>

// ============================================================================
// Layer: 单个图层。image 恒为 ARGB32_Premultiplied，尺寸与文档一致，初始透明。
// ============================================================================
enum class Blend { Normal, Multiply, Screen };

struct Layer
{
    QString name;
    QImage  image;
    bool    visible = true;
    int     opacity = 255;   // 0..255
    Blend   blend   = Blend::Normal;
};

// ============================================================================
// Document: 多图层文档模型（纯数据，无 UI 依赖）。
// layers 索引 0 = 最底层，末尾 = 最顶层。依赖 QImage 隐式共享，
// 因此整文档拷贝(用于撤销快照)几乎零成本，只有真正绘制才 detach 对应图层。
// ============================================================================
class Document
{
public:
    Document() = default;
    Document(int w, int h, const QString &baseName = QStringLiteral("背景"));

    int width() const { return m_width; }
    int height() const { return m_height; }
    QSize size() const { return QSize(m_width, m_height); }
    bool isNull() const { return m_width <= 0 || m_height <= 0 || m_layers.isEmpty(); }

    // ---- 图层访问 ----
    int layerCount() const { return m_layers.size(); }
    const QVector<Layer> &layers() const { return m_layers; }
    QVector<Layer> &layers() { return m_layers; }
    const Layer &layerAt(int i) const { return m_layers.at(i); }
    Layer &layerAt(int i) { return m_layers[i]; }

    int activeIndex() const { return m_active; }
    void setActiveIndex(int i);
    Layer *activeLayer();                 // 顶层索引即“最新添加”
    const Layer *activeLayer() const;
    QString activeLayerName() const;

    // ---- 合成 ----
    QImage composite() const;             // 自底向上叠加可见图层，按 opacity 混合

    // ---- 结构操作 ----
    void addLayer(const QString &name = QString(), const QColor &fill = Qt::transparent);
    void addImageLayer(const QImage &img, const QString &name = QString()); // 导入图片为新图层(居中)
    void insertLayerAboveActive(const QString &name = QString());
    void removeLayer(int i);
    void duplicateLayer(int i);
    void moveLayer(int from, int to);
    void setLayerVisible(int i, bool v);
    void setLayerOpacity(int i, int opacity);
    void setLayerBlend(int i, Blend b);
    void renameLayer(int i, const QString &name);
    void clearLayer(int i);               // 置为全透明
    void clearLayerTo(int i, const QColor &color);

    // ---- 几何操作（作用于所有图层，保持对齐）----
    void resizeAll(const QSize &newSize);              // 拉伸所有图层到新尺寸
    void cropAll(const QRect &rect);                   // 裁剪所有图层
    void rotateAll90(bool clockwise);
    void rotateAll(double degrees, const QColor &bg);  // 任意角度，所有图层同步
    void flipAll(bool horizontal, bool vertical);
    void resizeCanvasAll(const QSize &newSize, int anchor, const QColor &bg);

    // 从单张图创建（作为唯一图层）
    static Document fromImage(const QImage &img, const QString &name = QStringLiteral("背景"));

private:
    void clampActive();

    int m_width = 0;
    int m_height = 0;
    QVector<Layer> m_layers;
    int m_active = 0;   // 活动图层（默认最顶层）
};
