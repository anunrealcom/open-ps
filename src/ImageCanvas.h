#pragma once

#include <QWidget>
#include <QImage>
#include <QPolygonF>
#include <QColor>
#include <QPoint>
#include <QPointF>

class Document;

// ============================================================================
// ImageCanvas: 画布。渲染 Document 的合成结果，处理缩放/平移与各类画图工具交互。
// 所有对图层的修改都通过 commitLayerEdit 交给主窗口走撤销栈（传入编辑前的图层图像）。
// ============================================================================
class ImageCanvas : public QWidget
{
    Q_OBJECT
public:
    enum class Tool {
        Pan,        // 移动视图
        Crop,       // 裁剪框
        Brush,      // 画笔（抗锯齿圆头）
        Pencil,     // 铅笔（1px 硬边）
        Eraser,     // 橡皮
        Line,       // 直线
        Rect,       // 矩形
        Ellipse,    // 椭圆
        Fill,       // 油漆桶
        Picker,     // 取色器
        TextTool,   // 文字
        MaskBrush,  // 水印遮罩画笔
        MaskEraser, // 水印遮罩橡皮
        MaskRect    // 矩形遮罩
    };

    explicit ImageCanvas(QWidget *parent = nullptr);

    void setDocument(Document *doc);
    Document *document() const { return m_doc; }

    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    QString toolName() const;

    void setPenColor(const QColor &color);
    QColor penColor() const { return m_penColor; }
    void setPenWidth(int width);
    int penWidth() const { return m_penWidth; }
    void setBrushSize(int size);
    int brushSize() const { return m_brushSize; }

    void invalidate();   // 文档外部被改动后刷新合成缓存

    bool hasCropRect() const;
    QRect cropRect() const;      // 图像坐标
    void clearCropRect();

    QImage mask() const { return m_mask; }
    bool maskIsEmpty() const;
    void clearMask();

    void setCompareMode(bool on);

    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);
    void zoomIn()  { setZoom(m_zoom * 1.25); }
    void zoomOut() { setZoom(m_zoom / 1.25); }
    void zoomToFit();
    void zoomToActual() { setZoom(1.0); }

    QSize minimumSizeHint() const override { return QSize(240, 240); }

signals:
    // 一次图层编辑完成：layerIndex 处的图层从 beforeImage 变为当前值
    void commitLayerEdit(int layerIndex, const QImage &beforeImage, const QString &description);
    void applyCropRequested();
    void textRequested(const QPointF &imagePos);
    void colorPicked(const QColor &color);
    void fileDropped(const QString &filePath);
    void zoomChanged(qreal zoom);
    void cursorMoved(const QPoint &imagePos, bool valid);
    void cropRectChanged(bool hasRect);
    void maskChanged(bool hasMask);
    void maskRectFinished();   // 完成一次矩形遮罩框选（供“框选去水印”一步式使用）

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QPointF widgetToImage(const QPointF &p) const;
    QPointF imageToWidget(const QPointF &p) const;
    bool imageContains(const QPointF &p) const;
    void centerImage();
    void rebuildComposite();

    void drawStrokeSegment(const QPointF &a, const QPointF &b); // 画笔/铅笔/橡皮 实时
    void paintMaskStroke(const QPointF &a, const QPointF &b);
    void paintMaskRect(const QRectF &rect);
    void ensureMask();
    void commitShape();
    void updateToolCursor();
    void emitHover(const QPointF &widgetPos);

    Document *m_doc = nullptr;

    QImage m_composite;      // 合成缓存
    bool m_compositeDirty = true;
    QImage m_original;       // 对比模式：打开时的合成图
    bool m_compare = false;

    qreal m_zoom = 1.0;
    QPointF m_offset;

    Tool m_tool = Tool::Pan;
    QColor m_penColor = Qt::black;
    int m_penWidth = 4;
    int m_brushSize = 16;

    QImage m_mask;
    QImage m_maskOverlay;

    bool m_drawing = false;
    bool m_editing = false;         // 正在一次笔画中
    QImage m_editBefore;            // 笔画开始时的活动图层快照
    QPointF m_dragStart, m_dragEnd;
    QPolygonF m_stroke;
    QPointF m_lastDrawPos;
    QRect m_cropRect;

    bool m_panning = false;
    QPointF m_lastPanPos;
    QPointF m_hoverPos;
    bool m_hovering = false;
};
