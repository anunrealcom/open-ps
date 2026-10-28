#include "ImageCanvas.h"
#include "Document.h"
#include "ImageOps.h"

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QCursor>
#include <QPainterPath>
#include <algorithm>

namespace {
qreal clampZoom(qreal z) { return qBound(0.02, z, 32.0); }
}

ImageCanvas::ImageCanvas(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAcceptDrops(true);
    setMinimumSize(240, 240);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    updateToolCursor();
}

// ---------------------------------------------------------------------------
// 文档 / 刷新
// ---------------------------------------------------------------------------

void ImageCanvas::setDocument(Document *doc)
{
    m_doc = doc;
    clearMask();
    m_cropRect = QRect();
    if (m_doc) {
        m_original = m_doc->composite();
        m_compositeDirty = true;
        centerImage();
    } else {
        m_composite = QImage();
    }
    update();
}

void ImageCanvas::invalidate()
{
    m_compositeDirty = true;
    if (m_doc && m_cropRect.isValid() && !QRect(0, 0, m_doc->width(), m_doc->height()).contains(m_cropRect))
        clearCropRect();
    update();
}

void ImageCanvas::rebuildComposite()
{
    if (m_doc && !m_doc->isNull())
        m_composite = m_doc->composite();
    else
        m_composite = QImage();
    m_compositeDirty = false;
}

// ---------------------------------------------------------------------------
// 工具 / 画笔属性
// ---------------------------------------------------------------------------

void ImageCanvas::setTool(Tool tool)
{
    if (m_tool == tool)
        return;
    m_tool = tool;
    m_drawing = false;
    updateToolCursor();
    update();
}

QString ImageCanvas::toolName() const
{
    switch (m_tool) {
    case Tool::Pan:        return tr("移动视图");
    case Tool::Crop:       return tr("裁剪");
    case Tool::Brush:      return tr("画笔");
    case Tool::Pencil:     return tr("铅笔");
    case Tool::Eraser:     return tr("橡皮");
    case Tool::Line:       return tr("直线");
    case Tool::Rect:       return tr("矩形");
    case Tool::Ellipse:    return tr("椭圆");
    case Tool::Fill:       return tr("油漆桶");
    case Tool::Picker:     return tr("取色器");
    case Tool::TextTool:   return tr("文字");
    case Tool::MaskBrush:  return tr("遮罩画笔");
    case Tool::MaskEraser: return tr("遮罩橡皮");
    case Tool::MaskRect:   return tr("矩形遮罩");
    }
    return QString();
}

void ImageCanvas::setPenColor(const QColor &color) { m_penColor = color; update(); }
void ImageCanvas::setPenWidth(int width) { m_penWidth = qMax(1, width); update(); }
void ImageCanvas::setBrushSize(int size) { m_brushSize = qBound(1, size, 800); update(); }

bool ImageCanvas::hasCropRect() const
{
    return m_cropRect.isValid() && m_cropRect.width() > 2 && m_cropRect.height() > 2;
}

QRect ImageCanvas::cropRect() const
{
    if (!m_doc) return QRect();
    return m_cropRect.intersected(QRect(0, 0, m_doc->width(), m_doc->height()));
}

void ImageCanvas::clearCropRect()
{
    m_cropRect = QRect();
    m_drawing = false;
    emit cropRectChanged(false);
    update();
}

bool ImageCanvas::maskIsEmpty() const
{
    if (m_mask.isNull())
        return true;
    for (int y = 0; y < m_mask.height(); ++y) {
        const quint8 *line = reinterpret_cast<const quint8 *>(m_mask.constScanLine(y));
        for (int x = 0; x < m_mask.width(); ++x)
            if (line[x] > 128)
                return false;
    }
    return true;
}

void ImageCanvas::clearMask()
{
    m_mask = QImage();
    m_maskOverlay = QImage();
    emit maskChanged(false);
    update();
}

void ImageCanvas::setCompareMode(bool on) { m_compare = on; update(); }

void ImageCanvas::setZoom(qreal z)
{
    if (!m_doc || m_doc->isNull())
        return;
    const QPointF viewCenter = QPointF(width(), height()) / 2.0;
    const QPointF imgCenter = widgetToImage(viewCenter);
    m_zoom = clampZoom(z);
    m_offset = viewCenter - imgCenter * m_zoom;
    emit zoomChanged(m_zoom);
    update();
}

void ImageCanvas::zoomToFit()
{
    if (!m_doc || m_doc->isNull())
        return;
    const qreal zw = (width() - 24) / qreal(m_doc->width());
    const qreal zh = (height() - 24) / qreal(m_doc->height());
    m_zoom = clampZoom(qMin(1.0, qMin(zw, zh)));
    centerImage();
    emit zoomChanged(m_zoom);
    update();
}

// ---------------------------------------------------------------------------
// 坐标换算
// ---------------------------------------------------------------------------

QPointF ImageCanvas::widgetToImage(const QPointF &p) const
{
    return (p - m_offset) / m_zoom;
}

QPointF ImageCanvas::imageToWidget(const QPointF &p) const
{
    return p * m_zoom + m_offset;
}

bool ImageCanvas::imageContains(const QPointF &p) const
{
    if (!m_doc) return false;
    return p.x() >= 0 && p.y() >= 0 && p.x() < m_doc->width() && p.y() < m_doc->height();
}

void ImageCanvas::centerImage()
{
    if (!m_doc || m_doc->isNull()) { m_offset = QPointF(0, 0); return; }
    m_offset = (QPointF(width(), height())
                - QPointF(m_doc->width(), m_doc->height()) * m_zoom) / 2.0;
}

// ---------------------------------------------------------------------------
// 绘制到活动图层
// ---------------------------------------------------------------------------

void ImageCanvas::drawStrokeSegment(const QPointF &a, const QPointF &b)
{
    if (!m_doc || !m_doc->activeLayer())
        return;
    QImage &img = m_doc->activeLayer()->image;
    QPainter p(&img);
    if (m_tool == Tool::Eraser) {
        p.setCompositionMode(QPainter::CompositionMode_Clear);
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(Qt::black, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
    } else if (m_tool == Tool::Pencil) {
        p.setRenderHint(QPainter::Antialiasing, false);
        QPen pen(m_penColor, 1, Qt::SolidLine, Qt::SquareCap, Qt::MiterJoin);
        p.setPen(pen);
    } else { // Brush
        p.setRenderHint(QPainter::Antialiasing, true);
        QPen pen(m_penColor, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
    }
    p.drawLine(a, b);
    p.end();
    m_compositeDirty = true;
}

void ImageCanvas::commitShape()
{
    if (!m_doc || !m_doc->activeLayer())
        return;
    QImage &img = m_doc->activeLayer()->image;
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    const QRectF rect = QRectF(m_dragStart, m_dragEnd).normalized();
    QString desc;
    switch (m_tool) {
    case Tool::Line:    p.drawLine(m_dragStart, m_dragEnd); desc = tr("绘制直线"); break;
    case Tool::Rect:    p.drawRect(rect);   desc = tr("绘制矩形"); break;
    case Tool::Ellipse: p.drawEllipse(rect); desc = tr("绘制椭圆"); break;
    default: p.end(); return;
    }
    p.end();
    m_compositeDirty = true;
    emit commitLayerEdit(m_doc->activeIndex(), m_editBefore, desc);
}

// ---------------------------------------------------------------------------
// 遮罩
// ---------------------------------------------------------------------------

void ImageCanvas::ensureMask()
{
    if (!m_doc) return;
    if (m_mask.isNull() || m_mask.size() != m_doc->size()) {
        m_mask = QImage(m_doc->size(), QImage::Format_Grayscale8);
        m_mask.fill(0);
        m_maskOverlay = QImage(m_doc->size(), QImage::Format_ARGB32_Premultiplied);
        m_maskOverlay.fill(Qt::transparent);
    }
}

void ImageCanvas::paintMaskStroke(const QPointF &a, const QPointF &b)
{
    if (!m_doc) return;
    ensureMask();
    const bool erase = (m_tool == Tool::MaskEraser);
    QPen pen(Qt::white, m_brushSize, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    {
        QPainter pm(&m_mask);
        pm.setRenderHint(QPainter::Antialiasing, false);
        pen.setColor(erase ? Qt::black : Qt::white);
        pm.setPen(pen);
        pm.drawLine(a, b);
    }
    {
        QPainter po(&m_maskOverlay);
        po.setRenderHint(QPainter::Antialiasing, true);
        if (erase) {
            po.setCompositionMode(QPainter::CompositionMode_Clear);
            pen.setColor(Qt::white);
        } else {
            pen.setColor(QColor(255, 40, 40, 110));
        }
        po.setPen(pen);
        po.drawLine(a, b);
    }
    update();
}

void ImageCanvas::paintMaskRect(const QRectF &rect)
{
    if (!m_doc || rect.isEmpty()) return;
    ensureMask();
    const QRect r = rect.normalized().toRect()
                        .intersected(QRect(0, 0, m_doc->width(), m_doc->height()));
    if (r.isEmpty()) return;
    {
        QPainter pm(&m_mask);
        pm.fillRect(r, Qt::white);
    }
    {
        QPainter po(&m_maskOverlay);
        po.fillRect(r, QColor(255, 40, 40, 110));
    }
    update();
}

void ImageCanvas::updateToolCursor()
{
    switch (m_tool) {
    case Tool::Pan:        setCursor(Qt::OpenHandCursor); break;
    case Tool::TextTool:   setCursor(Qt::IBeamCursor); break;
    case Tool::Picker:     setCursor(Qt::CrossCursor); break;
    case Tool::MaskBrush:
    case Tool::MaskEraser: setCursor(Qt::BlankCursor); break;
    default:               setCursor(Qt::CrossCursor); break;
    }
}

void ImageCanvas::emitHover(const QPointF &widgetPos)
{
    if (!m_doc || m_doc->isNull()) return;
    const QPointF ip = widgetToImage(widgetPos);
    emit cursorMoved(QPoint(int(ip.x()), int(ip.y())), imageContains(ip));
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void ImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(0xFF, 0xFF, 0xFF));   // 编辑区底色：白色

    if (!m_doc || m_doc->isNull()) {
        p.setPen(QColor(0xA8, 0xAF, 0xB8));
        p.drawText(rect(), Qt::AlignCenter,
                   tr("将图片拖拽到此处，或按 Ctrl+O 打开图片"));
        return;
    }

    if (m_compare && !m_original.isNull()) {
        const QRectF target(m_offset, QSizeF(m_original.width(), m_original.height()) * m_zoom);
        p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
        p.drawImage(target, m_original);
        p.fillRect(QRectF(target.left(), target.top(), 64, 22), QColor(0, 0, 0, 160));
        p.setPen(Qt::white);
        p.drawText(QRectF(target.left(), target.top(), 64, 22), Qt::AlignCenter, tr("原图"));
        return;
    }

    if (m_compositeDirty)
        rebuildComposite();

    const QRectF target(m_offset, QSizeF(m_doc->width(), m_doc->height()) * m_zoom);
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 1.0);
    p.drawImage(target, m_composite);
    p.setPen(QPen(QColor(0, 0, 0, 140), 1));
    p.drawRect(target.adjusted(0, 0, -1, -1));

    // 遮罩
    if (!m_maskOverlay.isNull() && m_maskOverlay.size() == m_doc->size())
        p.drawImage(target, m_maskOverlay);

    if (m_drawing && m_tool == Tool::MaskRect) {
        const QRectF r = QRectF(m_dragStart, m_dragEnd).normalized();
        if (!r.isEmpty()) {
            const QRectF wr(imageToWidget(r.topLeft()), r.size() * m_zoom);
            p.fillRect(wr, QColor(255, 40, 40, 90));
            p.setPen(QPen(QColor(255, 90, 90), 1, Qt::DashLine));
            p.drawRect(wr);
        }
    }

    // 裁剪框
    if (m_tool == Tool::Crop && (m_drawing || hasCropRect())) {
        QRect cropImg = m_drawing
            ? QRectF(m_dragStart, m_dragEnd).normalized().toRect()
            : m_cropRect;
        cropImg = cropImg.intersected(QRect(0, 0, m_doc->width(), m_doc->height()));
        if (cropImg.width() > 2 && cropImg.height() > 2) {
            const QRectF wr(imageToWidget(cropImg.topLeft()),
                            QSizeF(cropImg.width(), cropImg.height()) * m_zoom);
            QPainterPath path;
            path.addRect(QRectF(rect()));
            path.addRect(wr);
            p.fillPath(path, QColor(0, 0, 0, 110));
            p.setPen(QPen(Qt::white, 1));
            p.drawRect(wr.adjusted(0, 0, -1, -1));
            p.setPen(QPen(QColor(255, 255, 255, 140), 1, Qt::DashLine));
            for (int i = 1; i <= 2; ++i) {
                p.drawLine(QPointF(wr.left() + wr.width() * i / 3.0, wr.top()),
                           QPointF(wr.left() + wr.width() * i / 3.0, wr.bottom()));
                p.drawLine(QPointF(wr.left(), wr.top() + wr.height() * i / 3.0),
                           QPointF(wr.right(), wr.top() + wr.height() * i / 3.0));
            }
        }
    }

    // 形状预览
    if (m_drawing && (m_tool == Tool::Line || m_tool == Tool::Rect || m_tool == Tool::Ellipse)) {
        p.save();
        p.translate(m_offset);
        p.scale(m_zoom, m_zoom);
        QPen pen(m_penColor, m_penWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        const QRectF rect = QRectF(m_dragStart, m_dragEnd).normalized();
        if (m_tool == Tool::Line) p.drawLine(m_dragStart, m_dragEnd);
        else if (m_tool == Tool::Rect) p.drawRect(rect);
        else p.drawEllipse(rect);
        p.restore();
    }

    // 圆形光标（画笔/橡皮/遮罩）
    if (m_hovering && (m_tool == Tool::Brush || m_tool == Tool::Eraser
                       || m_tool == Tool::MaskBrush || m_tool == Tool::MaskEraser)) {
        const qreal rad = m_brushSize * m_zoom / 2.0;
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 3));
        p.drawEllipse(m_hoverPos, rad, rad);
        p.setPen(QPen(Qt::white, 1));
        p.drawEllipse(m_hoverPos, rad, rad);
    }
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------

void ImageCanvas::mousePressEvent(QMouseEvent *e)
{
    if (!m_doc || m_doc->isNull())
        return;
    setFocus();
    const QPointF wp = e->position();
    const QPointF ip = widgetToImage(wp);

    if (e->button() == Qt::MiddleButton
        || (e->button() == Qt::LeftButton && m_tool == Tool::Pan)) {
        m_panning = true;
        m_lastPanPos = wp;
        if (m_tool == Tool::Pan) setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;

    switch (m_tool) {
    case Tool::Brush:
    case Tool::Pencil:
    case Tool::Eraser:
        if (!m_doc->activeLayer()) return;
        m_editing = true;
        m_drawing = true;
        m_editBefore = m_doc->activeLayer()->image;   // 浅拷贝，绘制时自动 detach
        m_lastDrawPos = ip;
        drawStrokeSegment(ip, ip + QPointF(0.0001, 0.0001));
        update();
        break;
    case Tool::Line:
    case Tool::Rect:
    case Tool::Ellipse:
        if (!m_doc->activeLayer()) return;
        m_drawing = true;
        m_editing = true;
        m_editBefore = m_doc->activeLayer()->image;
        m_dragStart = m_dragEnd = ip;
        update();
        break;
    case Tool::Fill: {
        if (!m_doc->activeLayer() || !imageContains(ip)) return;
        const int idx = m_doc->activeIndex();
        QImage before = m_doc->activeLayer()->image;
        ImageOps::floodFill(m_doc->activeLayer()->image, int(ip.x()), int(ip.y()),
                            m_penColor, 32, false);
        m_compositeDirty = true;
        emit commitLayerEdit(idx, before, tr("油漆桶填充"));
        update();
        break;
    }
    case Tool::Picker: {
        if (m_compositeDirty) rebuildComposite();
        if (imageContains(ip) && ip.x() >= 0 && ip.y() >= 0
            && int(ip.x()) < m_composite.width() && int(ip.y()) < m_composite.height())
            emit colorPicked(QColor(m_composite.pixel(int(ip.x()), int(ip.y()))));
        break;
    }
    case Tool::TextTool:
        if (imageContains(ip))
            emit textRequested(ip);
        break;
    case Tool::Crop:
        m_drawing = true;
        m_dragStart = m_dragEnd = ip;
        update();
        break;
    case Tool::MaskRect:
        m_drawing = true;
        m_dragStart = m_dragEnd = ip;
        update();
        break;
    case Tool::MaskBrush:
    case Tool::MaskEraser:
        m_drawing = true;
        m_lastDrawPos = ip;
        paintMaskStroke(ip, ip);
        emit maskChanged(true);
        break;
    default:
        break;
    }
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *e)
{
    m_hovering = true;
    m_hoverPos = e->position();
    emitHover(e->position());

    if (m_panning) {
        m_offset += e->position() - m_lastPanPos;
        m_lastPanPos = e->position();
        update();
        return;
    }
    if (!m_doc || m_doc->isNull()) return;
    const bool roundCursor = (m_tool == Tool::Brush || m_tool == Tool::Eraser
                              || m_tool == Tool::MaskBrush || m_tool == Tool::MaskEraser);
    if (roundCursor) update();

    if (!m_drawing)
        return;

    const QPointF ip = widgetToImage(e->position());
    switch (m_tool) {
    case Tool::Brush:
    case Tool::Pencil:
    case Tool::Eraser:
        drawStrokeSegment(m_lastDrawPos, ip);
        m_lastDrawPos = ip;
        update();
        break;
    case Tool::Line:
    case Tool::Rect:
    case Tool::Ellipse:
    case Tool::Crop:
    case Tool::MaskRect:
        m_dragEnd = ip;
        update();
        break;
    case Tool::MaskBrush:
    case Tool::MaskEraser:
        paintMaskStroke(m_lastDrawPos, ip);
        m_lastDrawPos = ip;
        break;
    default:
        break;
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::MiddleButton
        || (e->button() == Qt::LeftButton && m_tool == Tool::Pan)) {
        m_panning = false;
        updateToolCursor();
        update();
        return;
    }
    if (e->button() != Qt::LeftButton || !m_drawing)
        return;

    m_drawing = false;
    const QPointF ip = widgetToImage(e->position());
    switch (m_tool) {
    case Tool::Brush:
    case Tool::Pencil:
    case Tool::Eraser:
        if (m_editing) {
            m_editing = false;
            emit commitLayerEdit(m_doc->activeIndex(), m_editBefore,
                                 m_tool == Tool::Eraser ? tr("橡皮擦除")
                                 : (m_tool == Tool::Pencil ? tr("铅笔绘制") : tr("画笔绘制")));
        }
        break;
    case Tool::Line:
    case Tool::Rect:
    case Tool::Ellipse:
        m_dragEnd = ip;
        if (m_editing) { m_editing = false; commitShape(); }
        break;
    case Tool::Crop: {
        const QRect r = QRectF(m_dragStart, ip).normalized().toRect()
                            .intersected(QRect(0, 0, m_doc->width(), m_doc->height()));
        if (r.width() > 2 && r.height() > 2) { m_cropRect = r; emit cropRectChanged(true); }
        else clearCropRect();
        break;
    }
    case Tool::MaskRect:
        paintMaskRect(QRectF(m_dragStart, ip).normalized());
        emit maskChanged(true);
        emit maskRectFinished();
        break;
    default:
        break;
    }
    update();
}

void ImageCanvas::wheelEvent(QWheelEvent *e)
{
    if (!m_doc || m_doc->isNull()) { e->ignore(); return; }
    if (e->modifiers() & Qt::ControlModifier) {
        const qreal factor = e->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
        const QPointF anchor = e->position();
        const QPointF img = widgetToImage(anchor);
        m_zoom = clampZoom(m_zoom * factor);
        m_offset = anchor - img * m_zoom;
        emit zoomChanged(m_zoom);
        update();
        e->accept();
    } else {
        const QPoint d = e->angleDelta();
        if (!d.isNull()) {
            m_offset.rx() -= d.x() / 4.0;
            m_offset.ry() += d.y() / 4.0;
            update();
        }
        e->accept();
    }
}

void ImageCanvas::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (m_tool == Tool::Crop && hasCropRect()) {
            emit applyCropRequested();
            e->accept();
            return;
        }
    } else if (e->key() == Qt::Key_Escape) {
        if (m_drawing) { m_drawing = false; m_editing = false; }
        clearCropRect();
        e->accept();
        return;
    }
    QWidget::keyPressEvent(e);
}

void ImageCanvas::dragEnterEvent(QDragEnterEvent *e)
{
    const QMimeData *md = e->mimeData();
    if (md->hasUrls() && md->urls().size() == 1 && md->urls().first().isLocalFile())
        e->acceptProposedAction();
    else
        e->ignore();
}

void ImageCanvas::dropEvent(QDropEvent *e)
{
    const QMimeData *md = e->mimeData();
    if (md->hasUrls() && md->urls().size() == 1 && md->urls().first().isLocalFile()) {
        e->acceptProposedAction();
        emit fileDropped(md->urls().first().toLocalFile());
        return;
    }
    e->ignore();
}

void ImageCanvas::leaveEvent(QEvent *e)
{
    m_hovering = false;
    emit cursorMoved(QPoint(), false);
    update();
    QWidget::leaveEvent(e);
}
