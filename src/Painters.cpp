#include "Painters.h"

#include <QPainter>
#include <QJsonArray>
#include <QtMath>
#include <vector>

namespace {

// xorshift32 确定性随机数，保证同 seed 同结果
struct Rng {
    quint32 s;
    explicit Rng(quint32 seed) : s(seed ? seed : 0x9E3779B9u) {}
    quint32 next() {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return s;
    }
    double d01() { return next() / 4294967296.0; }
    int range(int lo, int hi) { return lo + int(next() % quint32(hi - lo + 1)); }
};

// HSV -> RGB（h 0..360, s/v 0..1）
QColor hsv(double h, double s, double v, int a = 255)
{
    h = std::fmod(h, 360.0); if (h < 0) h += 360.0;
    const double c = v * s;
    const double x = c * (1 - std::fabs(std::fmod(h / 60.0, 2.0) - 1));
    const double m = v - c;
    double r = 0, g = 0, b = 0;
    if (h < 60) { r = c; g = x; }
    else if (h < 120) { r = x; g = c; }
    else if (h < 180) { g = c; b = x; }
    else if (h < 240) { g = x; b = c; }
    else if (h < 300) { r = x; b = c; }
    else { r = c; b = x; }
    return QColor(qBound(0, int((r + m) * 255 + 0.5), 255),
                  qBound(0, int((g + m) * 255 + 0.5), 255),
                  qBound(0, int((b + m) * 255 + 0.5), 255), a);
}

} // namespace

namespace Painters {

// --------------------------------------------------------------------------
// Plasma：正弦叠加值噪声
// --------------------------------------------------------------------------
void plasma(QImage &image, quint32 seed, double scale)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    const double f1 = scale * (0.6 + rng.d01());
    const double f2 = scale * (0.6 + rng.d01());
    const double f3 = scale * (0.4 + rng.d01() * 0.8);
    const double f4 = scale * (0.3 + rng.d01() * 0.6);
    const double hueBase = rng.d01() * 360.0;
    const int w = image.width(), h = image.height();
    for (int y = 0; y < h; ++y) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < w; ++x) {
            double v = std::sin(x * f1) + std::sin(y * f2)
                     + std::sin((x + y) * f3)
                     + std::sin(std::sqrt(x * x + y * y) * f4);
            v = (v + 4.0) / 8.0;   // 0..1
            line[x] = hsv(hueBase + v * 220.0, 0.75, 0.55 + 0.45 * v).rgba();
        }
    }
}

// --------------------------------------------------------------------------
// Mandelbrot / Julia 分形着色
// --------------------------------------------------------------------------
void mandelbrot(QImage &image, quint32 seed, double zoom)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    const double hueBase = rng.d01() * 360.0;
    const int w = image.width(), h = image.height();
    const double maxIter = 128;
    const double span = 3.2 / zoom;
    const double cx0 = -0.7, cy0 = 0.0;
    for (int py = 0; py < h; ++py) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(py));
        for (int px = 0; px < w; ++px) {
            double cr = cx0 + (px / double(w) - 0.5) * span;
            double ci = cy0 + (py / double(h) - 0.5) * span * (h / double(w));
            double zr = 0, zi = 0; int i = 0;
            while (zr * zr + zi * zi <= 4.0 && i < int(maxIter)) {
                const double t = zr * zr - zi * zi + cr;
                zi = 2 * zr * zi + ci; zr = t; ++i;
            }
            if (i >= int(maxIter)) { line[px] = qRgba(10, 10, 20, 255); continue; }
            const double nu = i + 1 - std::log2(std::log(std::sqrt(zr * zr + zi * zi)) / std::log(2.0));
            line[px] = hsv(hueBase + nu * 8.0, 0.8, 0.6).rgba();
        }
    }
}

void julia(QImage &image, quint32 seed, double cx, double cy)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    const double hueBase = rng.d01() * 360.0;
    const int w = image.width(), h = image.height();
    const int maxIter = 160;
    for (int py = 0; py < h; ++py) {
        QRgb *line = reinterpret_cast<QRgb *>(image.scanLine(py));
        for (int px = 0; px < w; ++px) {
            double zr = (px / double(w) - 0.5) * 3.2;
            double zi = (py / double(h) - 0.5) * 3.2 * (h / double(w));
            int i = 0;
            while (zr * zr + zi * zi <= 4.0 && i < maxIter) {
                const double t = zr * zr - zi * zi + cx;
                zi = 2 * zr * zi + cy; zr = t; ++i;
            }
            if (i >= maxIter) { line[px] = qRgba(8, 8, 16, 255); continue; }
            line[px] = hsv(hueBase + i * 6.0, 0.8, 0.65).rgba();
        }
    }
}

// --------------------------------------------------------------------------
// 随机抽象图形
// --------------------------------------------------------------------------
void randomShapes(QImage &image, quint32 seed, int count)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = image.width(), h = image.height();
    const double hueBase = rng.d01() * 360.0;
    for (int i = 0; i < count; ++i) {
        const QColor col = hsv(hueBase + rng.d01() * 160.0, 0.5 + rng.d01() * 0.5,
                               0.5 + rng.d01() * 0.5, rng.range(60, 180));
        p.setPen(Qt::NoPen);
        p.setBrush(col);
        const int kind = rng.range(0, 2);
        const int x = rng.range(0, w), y = rng.range(0, h);
        const int r = rng.range(w / 40, w / 6);
        if (kind == 0)
            p.drawEllipse(QPoint(x, y), r, r);
        else if (kind == 1)
            p.drawRect(x, y, r, rng.range(w / 40, w / 6));
        else {
            QPen lp(col, rng.range(1, 6), Qt::SolidLine, Qt::RoundCap);
            p.setPen(lp); p.setBrush(Qt::NoBrush);
            p.drawLine(x, y, rng.range(0, w), rng.range(0, h));
        }
    }
    p.end();
}

// --------------------------------------------------------------------------
// 流场笔触：基于噪声向量场画密集曲线，模仿油画笔触
// --------------------------------------------------------------------------
void flowField(QImage &image, quint32 seed, int particles, int steps)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    const int w = image.width(), h = image.height();
    const double scale = 0.006 + rng.d01() * 0.006;
    const double ox = rng.d01() * 1000, oy = rng.d01() * 1000;
    auto angleAt = [&](double x, double y) {
        // 廉价“噪声”：多正弦叠加
        return (std::sin(x * scale + ox) + std::cos(y * scale + oy)
              + std::sin((x + y) * scale * 0.5)) * 1.6;
    };
    const double hueBase = rng.d01() * 360.0;
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);
    for (int i = 0; i < particles; ++i) {
        double x = rng.d01() * w, y = rng.d01() * h;
        const QColor col = hsv(hueBase + rng.d01() * 140.0, 0.6, 0.7, 90);
        QPen pen(col, 1.0 + rng.d01() * 2.0, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        QPointF prev(x, y);
        for (int s = 0; s < steps; ++s) {
            const double a = angleAt(x, y);
            x += std::cos(a) * 2.0;
            y += std::sin(a) * 2.0;
            if (x < 0 || y < 0 || x >= w || y >= h) break;
            p.drawLine(prev, QPointF(x, y));
            prev = QPointF(x, y);
        }
    }
    p.end();
}

// --------------------------------------------------------------------------
// 渐变
// --------------------------------------------------------------------------
void gradient(QImage &image, quint32 seed, bool radial)
{
    if (image.format() != QImage::Format_ARGB32_Premultiplied)
        image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    Rng rng(seed);
    const int w = image.width(), h = image.height();
    const QColor a = hsv(rng.d01() * 360.0, 0.7, 0.8);
    const QColor b = hsv(rng.d01() * 360.0, 0.7, 0.5);
    QPainter p(&image);
    QBrush br;
    if (radial) {
        QRadialGradient g(QPointF(w / 2.0, h / 2.0), std::max(w, h) / 1.4);
        g.setColorAt(0, a); g.setColorAt(1, b);
        br = QBrush(g);
    } else {
        QLinearGradient g(0, 0, w, h);
        g.setColorAt(0, a); g.setColorAt(1, b);
        br = QBrush(g);
    }
    p.fillRect(QRect(0, 0, w, h), br);
    p.end();
}

// --------------------------------------------------------------------------
// 统一入口
// --------------------------------------------------------------------------
bool generate(QImage &image, const QString &kind, const QJsonObject &params)
{
    if (image.isNull())
        return false;
    const quint32 seed = quint32(qint64(params.value("seed").toDouble(1)) & 0xFFFFFFFF);
    if (kind == "plasma") {
        plasma(image, seed, params.value("scale").toDouble(0.02));
    } else if (kind == "mandelbrot") {
        mandelbrot(image, seed, params.value("zoom").toDouble(1.0));
    } else if (kind == "julia") {
        julia(image, seed, params.value("cx").toDouble(-0.8), params.value("cy").toDouble(0.156));
    } else if (kind == "shapes") {
        randomShapes(image, seed, int(params.value("count").toDouble(60)));
    } else if (kind == "flowfield") {
        flowField(image, seed, int(params.value("particles").toDouble(800)),
                  int(params.value("steps").toDouble(120)));
    } else if (kind == "gradient") {
        gradient(image, seed, params.value("radial").toBool(false));
    } else {
        return false;
    }
    return true;
}

} // namespace Painters
