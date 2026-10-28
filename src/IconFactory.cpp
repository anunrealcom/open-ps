#include "IconFactory.h"

#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <functional>

namespace {

// 在 32 逻辑网格上绘制各图标；外部已把 painter 缩放到目标尺寸。
void drawGlyph(QPainter &p, const QString &name, const QColor &c)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    QPen pen(c, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QPen thin(c, 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setBrush(Qt::NoBrush);

    auto stroke = [&](std::function<void(QPainter &)> fn) {
        p.save(); p.setPen(pen); p.setBrush(Qt::NoBrush); fn(p); p.restore();
    };
    auto fill = [&](std::function<void(QPainter &)> fn) {
        p.save(); p.setPen(Qt::NoPen); p.setBrush(c); fn(p); p.restore();
    };

    if (name == "open") {
        // 打开的文件夹
        QPainterPath body;
        body.moveTo(4, 25);
        body.lineTo(4, 10);
        body.lineTo(12, 10);
        body.lineTo(15, 13);
        body.lineTo(28, 13);
        body.lineTo(28, 25);
        body.closeSubpath();
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 36));
        p.drawPath(body);
        stroke([&](QPainter &q){
            q.drawLine(4, 25, 9, 16); q.drawLine(9, 16, 30, 16); q.drawLine(30, 16, 25, 25);
        });
    }
    else if (name == "save") {
        QPainterPath disk;
        disk.moveTo(6, 6); disk.lineTo(22, 6); disk.lineTo(26, 10);
        disk.lineTo(26, 26); disk.lineTo(6, 26); disk.closeSubpath();
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 30));
        p.drawPath(disk);
        p.setBrush(Qt::NoBrush);
        stroke([&](QPainter &q){ q.drawRect(11, 6, 10, 6); });          // 顶部滑块
        stroke([&](QPainter &q){ q.drawRect(10, 17, 12, 9); });         // 底部标签
    }
    else if (name == "undo") {
        stroke([&](QPainter &q){
            QPainterPath arc; arc.moveTo(9, 12);
            arc.cubicTo(18, 8, 26, 13, 24, 21);
            q.drawPath(arc);
        });
        stroke([&](QPainter &q){ q.drawLine(9, 12, 6, 8); q.drawLine(9, 12, 13, 10); });
    }
    else if (name == "redo") {
        stroke([&](QPainter &q){
            QPainterPath arc; arc.moveTo(23, 12);
            arc.cubicTo(14, 8, 6, 13, 8, 21);
            q.drawPath(arc);
        });
        stroke([&](QPainter &q){ q.drawLine(23, 12, 26, 8); q.drawLine(23, 12, 19, 10); });
    }
    else if (name == "crop") {
        stroke([&](QPainter &q){ q.drawLine(9, 4, 9, 23); q.drawLine(9, 23, 28, 23); });
        stroke([&](QPainter &q){ q.drawLine(4, 9, 23, 9); q.drawLine(23, 9, 23, 28); });
    }
    else if (name == "adjust") {
        // 三根滑杆 + 圆点
        for (int i = 0; i < 3; ++i) {
            const int y = 8 + i * 8;
            const int knob = (i == 0) ? 21 : (i == 1 ? 11 : 17);
            p.setPen(thin); p.drawLine(5, y, 27, y);
            p.setPen(Qt::NoPen); p.setBrush(c);
            p.drawEllipse(QPointF(knob, y), 2.6, 2.6);
        }
    }
    else if (name == "blur") {
        for (int i = 0; i < 4; ++i) {
            p.setPen(QPen(c, 2.0));
            p.setBrush(QColor(c.red(), c.green(), c.blue(), 40 + i * 12));
            p.drawEllipse(QPointF(16, 16), 4 + i * 2.6, 4 + i * 2.6);
        }
    }
    else if (name == "sharpen") {
        stroke([&](QPainter &q){
            QPainterPath t; t.moveTo(16, 5); t.lineTo(27, 27); t.lineTo(5, 27); t.closeSubpath();
            q.drawPath(t);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(16, 20), 2.0, 2.0); });
    }
    else if (name == "grayscale") {
        p.setPen(pen);
        p.setBrush(c); p.drawEllipse(QPointF(12, 16), 7, 7);
        p.setBrush(Qt::white); p.drawEllipse(QPointF(20, 16), 7, 7);
        p.setBrush(Qt::NoBrush);
    }
    else if (name == "invert") {
        p.setPen(pen);
        QPainterPath circle; circle.addEllipse(QPointF(16, 16), 9, 9);
        QPainterPath half; half.moveTo(16, 7); half.arcTo(QRectF(7, 7, 18, 18), 90, -180); half.closeSubpath();
        p.setBrush(c); p.drawPath(circle);
        p.setBrush(Qt::white); p.drawPath(half);
        p.setBrush(Qt::NoBrush); p.drawPath(circle);
    }
    else if (name == "levels") {
        stroke([&](QPainter &q){ q.drawRect(5, 8, 22, 16); });
        for (int i = 0; i < 4; ++i)
            fill([&](QPainter &q){ q.drawRect(8 + i * 5, 12 + (i % 2) * 3, 2.4, 9); });
    }
    else if (name == "rotate") {
        stroke([&](QPainter &q){
            QPainterPath arc; arc.moveTo(24, 12);
            arc.cubicTo(24, 22, 16, 26, 10, 22);
            q.drawPath(arc);
        });
        stroke([&](QPainter &q){ q.drawLine(24, 12, 20, 9); q.drawLine(24, 12, 27, 15); });
        stroke([&](QPainter &q){ q.drawRect(9, 9, 8, 8); });
    }
    else if (name == "flip") {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), 70));
        QPainterPath l; l.moveTo(13, 8); l.lineTo(5, 16); l.lineTo(13, 24); l.closeSubpath();
        p.drawPath(l);
        p.setBrush(c);
        QPainterPath r; r.moveTo(19, 8); r.lineTo(27, 16); r.lineTo(19, 24); r.closeSubpath();
        p.drawPath(r);
        p.setPen(thin); p.drawLine(16, 5, 16, 27);
    }
    else if (name == "resize") {
        stroke([&](QPainter &q){ q.drawRect(5, 5, 15, 15); });
        stroke([&](QPainter &q){ q.drawRect(12, 12, 15, 15); });
    }
    else if (name == "canvas") {
        stroke([&](QPainter &q){ q.drawRect(4, 4, 24, 24); });
        QPen dash = thin; dash.setStyle(Qt::DotLine);
        p.setPen(dash);
        p.setBrush(Qt::NoBrush);
        p.drawRect(10, 10, 12, 12);
    }
    else if (name == "watermark" || name == "mask") {
        // 魔法橡皮 + 星芒
        p.save();
        p.translate(16, 17); p.rotate(45);
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 40));
        p.drawRoundedRect(QRectF(-8, -4, 16, 8), 2, 2);
        p.drawLine(0, -4, 0, 4);
        p.restore();
        stroke([&](QPainter &q){
            q.drawLine(22, 5, 22, 11); q.drawLine(19, 8, 25, 8);
        });
    }
    else if (name == "pen") {
        stroke([&](QPainter &q){
            q.drawLine(8, 24, 20, 12);
            q.drawLine(20, 12, 24, 16);
            q.drawLine(24, 16, 12, 28);
            q.drawLine(12, 28, 8, 24);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(23, 10), 2.4, 2.4); });
    }
    else if (name == "line") {
        stroke([&](QPainter &q){ q.drawLine(6, 26, 26, 6); });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(6, 26), 2.2, 2.2); q.drawEllipse(QPointF(26, 6), 2.2, 2.2); });
    }
    else if (name == "rect") {
        stroke([&](QPainter &q){ q.drawRect(5, 8, 22, 16); });
    }
    else if (name == "ellipse") {
        stroke([&](QPainter &q){ q.drawEllipse(QRectF(5, 8, 22, 16)); });
    }
    else if (name == "text") {
        stroke([&](QPainter &q){
            q.drawLine(7, 8, 25, 8); q.drawLine(16, 8, 16, 25);
        });
        stroke([&](QPainter &q){ q.drawLine(11, 25, 21, 25); });
    }
    else if (name == "color") {
        // 调色板
        QPainterPath pal;
        pal.moveTo(16, 5); pal.cubicTo(27, 5, 28, 14, 24, 17);
        pal.cubicTo(21, 19, 22, 23, 18, 25); pal.cubicTo(10, 28, 4, 22, 5, 14);
        pal.cubicTo(6, 8, 11, 5, 16, 5);
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 30));
        p.drawPath(pal);
        p.setBrush(Qt::NoBrush);
        const QColor dots[3] = { QColor("#ff5b5b"), QColor("#ffd24a"), QColor("#4ad295") };
        for (int i = 0; i < 3; ++i) {
            p.setPen(Qt::NoPen); p.setBrush(dots[i]);
            p.drawEllipse(QPointF(10 + i * 5, 12 + (i % 2) * 4), 1.8, 1.8);
        }
    }
    else if (name == "zoomin") {
        stroke([&](QPainter &q){ q.drawEllipse(QPointF(14, 14), 8, 8); q.drawLine(20, 20, 27, 27); });
        stroke([&](QPainter &q){ q.drawLine(14, 10, 14, 18); q.drawLine(10, 14, 18, 14); });
    }
    else if (name == "zoomout") {
        stroke([&](QPainter &q){ q.drawEllipse(QPointF(14, 14), 8, 8); q.drawLine(20, 20, 27, 27); });
        stroke([&](QPainter &q){ q.drawLine(10, 14, 18, 14); });
    }
    else if (name == "fit") {
        stroke([&](QPainter &q){
            q.drawLine(5, 10, 5, 5);    q.drawLine(5, 5, 10, 5);
            q.drawLine(22, 5, 27, 5);   q.drawLine(27, 5, 27, 10);
            q.drawLine(27, 22, 27, 27); q.drawLine(27, 27, 22, 27);
            q.drawLine(10, 27, 5, 27);  q.drawLine(5, 27, 5, 22);
        });
    }
    else if (name == "actual") {
        stroke([&](QPainter &q){ q.drawRect(6, 6, 20, 20); });
        p.setPen(thin); p.drawText(QRectF(6, 6, 20, 20), Qt::AlignCenter, "1");
    }
    else if (name == "compare") {
        stroke([&](QPainter &q){ q.drawRect(5, 7, 22, 18); q.drawLine(16, 7, 16, 25); });
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(c.red(), c.green(), c.blue(), 60));
        p.drawRect(5, 7, 11, 18);
    }
    else if (name == "file") {
        stroke([&](QPainter &q){
            QPainterPath f; f.moveTo(9, 5); f.lineTo(19, 5); f.lineTo(24, 10);
            f.lineTo(24, 27); f.lineTo(9, 27); f.closeSubpath();
            q.drawPath(f); q.drawLine(19, 5, 19, 10); q.drawLine(19, 10, 24, 10);
        });
    }
    else if (name == "edit") {
        stroke([&](QPainter &q){ q.drawLine(7, 25, 21, 11); q.drawLine(21, 11, 25, 15); q.drawLine(25, 15, 11, 29); });
        stroke([&](QPainter &q){ q.drawLine(7, 25, 5, 29); q.drawLine(5, 29, 11, 29); });
    }
    else if (name == "image") {
        stroke([&](QPainter &q){ q.drawRect(4, 7, 24, 18); });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(11, 14), 2.2, 2.2); });
        stroke([&](QPainter &q){
            QPainterPath m; m.moveTo(6, 23); m.lineTo(13, 16); m.lineTo(18, 21);
            m.lineTo(22, 17); m.lineTo(26, 23);
            q.drawPath(m);
        });
    }
    else if (name == "filter") {
        stroke([&](QPainter &q){
            QPainterPath f; f.moveTo(5, 7); f.lineTo(27, 7); f.lineTo(18, 16);
            f.lineTo(18, 25); f.lineTo(14, 22); f.lineTo(14, 16); f.closeSubpath();
            q.drawPath(f);
        });
    }
    else if (name == "view") {
        stroke([&](QPainter &q){
            QPainterPath eye; eye.moveTo(4, 16);
            eye.cubicTo(10, 8, 22, 8, 28, 16);
            eye.cubicTo(22, 24, 10, 24, 4, 16);
            q.drawPath(eye);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(16, 16), 3.2, 3.2); });
    }
    else if (name == "draw") {
        stroke([&](QPainter &q){
            QPainterPath s; s.moveTo(6, 22);
            s.cubicTo(12, 26, 14, 12, 20, 12);
            s.cubicTo(25, 12, 24, 20, 27, 18);
            q.drawPath(s);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(27, 18), 2.0, 2.0); });
    }
    else if (name == "help") {
        stroke([&](QPainter &q){ q.drawEllipse(QPointF(16, 16), 11, 11); });
        p.setPen(QPen(c, 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawText(QRectF(4, 4, 24, 24), Qt::AlignCenter, "?");
    }
    else if (name == "info") {
        stroke([&](QPainter &q){ q.drawEllipse(QPointF(16, 16), 11, 11); });
        p.setPen(QPen(c, 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawText(QRectF(4, 4, 24, 24), Qt::AlignCenter, "i");
    }
    // ---- 新增：文件/图层/画图工具 ----
    else if (name == "new") {
        stroke([&](QPainter &q){
            QPainterPath f; f.moveTo(8, 5); f.lineTo(17, 5); f.lineTo(22, 10);
            f.lineTo(22, 27); f.lineTo(8, 27); f.closeSubpath();
            q.drawPath(f);
        });
        stroke([&](QPainter &q){ q.drawLine(17, 5, 17, 10); q.drawLine(17, 10, 22, 10); });
        stroke([&](QPainter &q){ q.drawLine(11, 19, 19, 19); q.drawLine(15, 15, 15, 23); });
    }
    else if (name == "copy") {
        stroke([&](QPainter &q){ q.drawRoundedRect(QRectF(6, 6, 13, 15), 2, 2); });
        stroke([&](QPainter &q){ q.drawRoundedRect(QRectF(13, 11, 13, 15), 2, 2); });
    }
    else if (name == "delete") {
        stroke([&](QPainter &q){ q.drawLine(7, 9, 25, 9); });
        stroke([&](QPainter &q){ q.drawLine(12, 9, 13, 6); q.drawLine(13, 6, 19, 6); q.drawLine(19, 6, 20, 9); });
        stroke([&](QPainter &q){
            QPainterPath body; body.moveTo(9, 9); body.lineTo(11, 27); body.lineTo(21, 27);
            body.lineTo(23, 9); q.drawPath(body);
        });
        p.setPen(thin); p.drawLine(13, 13, 13, 23); p.drawLine(16, 13, 16, 23); p.drawLine(19, 13, 19, 23);
    }
    else if (name == "layers") {
        auto diamond = [&](qreal oy){
            QPainterPath d; d.moveTo(16, oy - 5); d.lineTo(27, oy); d.lineTo(16, oy + 5);
            d.lineTo(5, oy); d.closeSubpath(); p.drawPath(d);
        };
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 24));
        diamond(22); diamond(16); p.setBrush(Qt::NoBrush); diamond(10);
    }
    else if (name == "pencil") {
        stroke([&](QPainter &q){
            q.drawLine(7, 25, 20, 12); q.drawLine(9, 27, 22, 14);
            q.drawLine(20, 12, 22, 14);
            q.drawLine(7, 25, 9, 27);
            q.drawLine(20, 12, 25, 7); q.drawLine(22, 14, 25, 7);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(8, 26), 1.4, 1.4); });
    }
    else if (name == "brush") {
        stroke([&](QPainter &q){ q.drawLine(24, 6, 14, 16); });
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 30));
        QPainterPath head; head.moveTo(14, 14); head.lineTo(18, 18);
        head.lineTo(10, 26); head.lineTo(6, 22); head.closeSubpath();
        p.drawPath(head);
        p.setBrush(Qt::NoBrush);
        fill([&](QPainter &q){ q.drawEllipse(QPointF(7, 25), 2.2, 2.2); });
    }
    else if (name == "eraser") {
        p.save();
        p.translate(16, 17); p.rotate(35);
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 30));
        p.drawRoundedRect(QRectF(-9, -6, 18, 12), 2, 2);
        p.drawLine(0, -6, 0, 6);
        p.restore();
        stroke([&](QPainter &q){ q.drawLine(6, 27, 26, 27); });
    }
    else if (name == "fill") {
        p.save();
        p.translate(15, 16); p.rotate(-30);
        p.setPen(pen); p.setBrush(QColor(c.red(), c.green(), c.blue(), 30));
        QPainterPath b; b.moveTo(-8, -6); b.lineTo(8, -6); b.lineTo(6, 8); b.lineTo(-6, 8); b.closeSubpath();
        p.drawPath(b);
        p.drawLine(-8, -6, -12, -6);
        p.restore();
        fill([&](QPainter &q){
            QPainterPath drop; drop.moveTo(24, 12);
            drop.cubicTo(28, 18, 27, 22, 24, 22); drop.cubicTo(21, 22, 20, 18, 24, 12);
            q.drawPath(drop);
        });
    }
    else if (name == "picker") {
        stroke([&](QPainter &q){
            q.drawLine(20, 6, 26, 12); q.drawLine(23, 9, 12, 20);
            q.drawLine(12, 20, 9, 23); q.drawLine(9, 23, 6, 26);
            q.drawLine(12, 20, 6, 26);
        });
        fill([&](QPainter &q){ q.drawEllipse(QPointF(6, 26), 2.2, 2.2); });
    }
    else if (name == "pan") {
        stroke([&](QPainter &q){
            q.drawLine(16, 6, 16, 26); q.drawLine(6, 16, 26, 16);
            q.drawLine(16, 6, 13, 9); q.drawLine(16, 6, 19, 9);
            q.drawLine(16, 26, 13, 23); q.drawLine(16, 26, 19, 23);
            q.drawLine(6, 16, 9, 13); q.drawLine(6, 16, 9, 19);
            q.drawLine(26, 16, 23, 13); q.drawLine(26, 16, 23, 19);
        });
    }
    else if (name == "arrow-up") {
        stroke([&](QPainter &q){ q.drawLine(16, 24, 16, 9); q.drawLine(16, 9, 10, 15); q.drawLine(16, 9, 22, 15); });
    }
    else if (name == "arrow-down") {
        stroke([&](QPainter &q){ q.drawLine(16, 8, 16, 23); q.drawLine(16, 23, 10, 17); q.drawLine(16, 23, 22, 17); });
    }
    else if (name == "clear") {
        // 扫帚/清空：垃圾桶 + 斜线
        stroke([&](QPainter &q){
            q.drawLine(7, 10, 25, 10);
            QPainterPath body; body.moveTo(9, 10); body.lineTo(11, 26); body.lineTo(21, 26);
            body.lineTo(23, 10); q.drawPath(body);
            q.drawLine(13, 6, 19, 6);
        });
        p.setPen(thin); p.drawLine(13, 14, 13, 22); p.drawLine(16, 14, 16, 22); p.drawLine(19, 14, 19, 22);
    }
    // ---- 窗口按钮（细线，颜色由调用方传入）----
    else if (name == "win-min") {
        p.setPen(QPen(c, 1.6)); p.drawLine(10, 22, 22, 22);
    }
    else if (name == "win-max") {
        p.setPen(QPen(c, 1.6)); p.drawRect(10, 10, 12, 12);
    }
    else if (name == "win-restore") {
        p.setPen(QPen(c, 1.6));
        p.drawRect(9, 12, 11, 11);
        p.drawLine(13, 12, 13, 9); p.drawLine(13, 9, 23, 9);
        p.drawLine(23, 9, 23, 19); p.drawLine(23, 19, 20, 19);
    }
    else if (name == "win-close") {
        p.setPen(QPen(c, 1.7, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(10, 10, 22, 22); p.drawLine(22, 10, 10, 22);
    }
    else {
        // 未知：画一个占位圆点，避免完全空白
        fill([&](QPainter &q){ q.drawEllipse(QPointF(16, 16), 4, 4); });
    }
}

} // namespace

namespace IconFactory {

QPixmap pixmap(const QString &name, int size, const QColor &color)
{
    const int dpr = 2;
    QPixmap pm(size * dpr, size * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    const qreal scale = size / 32.0;
    p.scale(scale, scale);
    drawGlyph(p, name, color);
    p.end();
    return pm;
}

QIcon icon(const QString &name, const QColor &color)
{
    QIcon ic;
    for (int s : {16, 20, 24, 32})
        ic.addPixmap(pixmap(name, s, color));
    return ic;
}

} // namespace IconFactory
