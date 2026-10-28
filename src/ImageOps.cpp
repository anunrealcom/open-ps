#include "ImageOps.h"

#include <QtGlobal>
#include <QTransform>
#include <QPainter>
#include <cmath>
#include <vector>
#include <algorithm>

namespace ImageOps {

// ============================================================================
// 色彩调整
// ============================================================================

QImage applyAdjustments(const QImage &srcIn, const AdjustParams &p)
{
    if (srcIn.isNull())
        return srcIn;

    QImage src = srcIn.convertToFormat(QImage::Format_ARGB32);
    QImage out = src;

    const double brightAdd = p.brightness * 1.28;                 // -128..128
    const double contrastF = (100.0 + p.contrast) / 100.0;        // 0..2
    const double satF      = (100.0 + p.saturation) / 100.0;      // 0..2
    const double tempShift = p.temperature * 0.5;                 // -50..50

    for (int y = 0; y < src.height(); ++y) {
        const QRgb *in  = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb       *o   = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb rgb = in[x];
            const int a = qAlpha(rgb);

            double r = qRed(rgb)   + brightAdd + tempShift;
            double g = qGreen(rgb) + brightAdd;
            double b = qBlue(rgb)  + brightAdd - tempShift;

            // 对比度（围绕中点）
            r = (r - 127.5) * contrastF + 127.5;
            g = (g - 127.5) * contrastF + 127.5;
            b = (b - 127.5) * contrastF + 127.5;

            // 饱和度（向亮度收敛或发散）
            const double lum = 0.299 * r + 0.587 * g + 0.114 * b;
            r = lum + (r - lum) * satF;
            g = lum + (g - lum) * satF;
            b = lum + (b - lum) * satF;

            o[x] = qRgba(qBound(0, int(std::lround(r)), 255),
                         qBound(0, int(std::lround(g)), 255),
                         qBound(0, int(std::lround(b)), 255),
                         a);
        }
    }
    return out;
}

QImage toGrayscale(const QImage &srcIn)
{
    if (srcIn.isNull())
        return srcIn;
    QImage src = srcIn.convertToFormat(QImage::Format_ARGB32);
    QImage out = src;
    for (int y = 0; y < src.height(); ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const int v = qGray(in[x]);
            o[x] = qRgba(v, v, v, qAlpha(in[x]));
        }
    }
    return out;
}

QImage invertColors(const QImage &srcIn)
{
    if (srcIn.isNull())
        return srcIn;
    QImage out = srcIn.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        QRgb *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QRgb rgb = o[x];
            o[x] = qRgba(255 - qRed(rgb), 255 - qGreen(rgb),
                         255 - qBlue(rgb), qAlpha(rgb));
        }
    }
    return out;
}

QImage autoLevels(const QImage &srcIn)
{
    if (srcIn.isNull())
        return srcIn;
    QImage src = srcIn.convertToFormat(QImage::Format_ARGB32);

    quint32 hist[3][256] = {};
    for (int y = 0; y < src.height(); ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            ++hist[0][qRed(in[x])];
            ++hist[1][qGreen(in[x])];
            ++hist[2][qBlue(in[x])];
        }
    }

    const quint32 total = quint32(src.width() * src.height());
    const quint32 clip = qMax<quint32>(1, total / 400); // 0.25% 双侧裁剪

    int lo[3], hi[3];
    for (int c = 0; c < 3; ++c) {
        quint32 acc = 0;
        int l = 0, h = 255;
        for (int v = 0; v < 256; ++v) {
            acc += hist[c][v];
            if (acc > clip) { l = v; break; }
        }
        acc = 0;
        for (int v = 255; v >= 0; --v) {
            acc += hist[c][v];
            if (acc > clip) { h = v; break; }
        }
        if (h <= l + 1) { l = 0; h = 255; } // 无有效范围则不拉伸
        lo[c] = l; hi[c] = h;
    }

    quint8 lut[3][256];
    for (int c = 0; c < 3; ++c) {
        const double scale = 255.0 / (hi[c] - lo[c]);
        for (int v = 0; v < 256; ++v) {
            int nv = int((v - lo[c]) * scale + 0.5);
            lut[c][v] = quint8(qBound(0, nv, 255));
        }
    }

    QImage out = src;
    for (int y = 0; y < src.height(); ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < src.width(); ++x) {
            const QRgb rgb = in[x];
            o[x] = qRgba(lut[0][qRed(rgb)], lut[1][qGreen(rgb)],
                         lut[2][qBlue(rgb)], qAlpha(rgb));
        }
    }
    return out;
}

// ============================================================================
// 滤镜：高斯模糊 / 锐化
// ============================================================================

namespace {

std::vector<float> gaussKernel(int radius)
{
    radius = qMax(1, radius);
    const int size = radius * 2 + 1;
    std::vector<float> k(size);
    const float sigma = std::max(1.0f, radius / 2.0f);
    float sum = 0.f;
    for (int i = 0; i < size; ++i) {
        const float d = float(i - radius);
        k[i] = std::exp(-(d * d) / (2.f * sigma * sigma));
        sum += k[i];
    }
    for (auto &v : k) v /= sum;
    return k;
}

// 单方向（水平/垂直）一维卷积
void blurPass(const QImage &src, QImage &dst, const std::vector<float> &k, int radius,
              bool horizontal)
{
    const int w = src.width(), h = src.height();
    for (int y = 0; y < h; ++y) {
        const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        QRgb *d = reinterpret_cast<QRgb *>(dst.scanLine(y));
        for (int x = 0; x < w; ++x) {
            float r = 0, g = 0, b = 0, a = 0;
            for (int t = -radius; t <= radius; ++t) {
                int sx = x, sy = y;
                if (horizontal) sx = qBound(0, x + t, w - 1);
                else            sy = qBound(0, y + t, h - 1);
                const QRgb px = reinterpret_cast<const QRgb *>(
                                    src.constScanLine(sy))[sx];
                const float wt = k[t + radius];
                r += qRed(px)   * wt;
                g += qGreen(px) * wt;
                b += qBlue(px)  * wt;
                a += qAlpha(px) * wt;
            }
            d[x] = qRgba(qBound(0, int(r + 0.5f), 255),
                         qBound(0, int(g + 0.5f), 255),
                         qBound(0, int(b + 0.5f), 255),
                         qBound(0, int(a + 0.5f), 255));
        }
    }
}

} // namespace

QImage gaussianBlur(const QImage &srcIn, int radius)
{
    if (srcIn.isNull() || radius < 1)
        return srcIn;
    QImage src = srcIn.convertToFormat(QImage::Format_ARGB32);
    const std::vector<float> k = gaussKernel(radius);
    QImage tmp(src.size(), QImage::Format_ARGB32);
    QImage out(src.size(), QImage::Format_ARGB32);
    blurPass(src, tmp, k, radius, true);
    blurPass(tmp, out, k, radius, false);
    return out;
}

QImage sharpen(const QImage &src, int radius, double amount)
{
    if (src.isNull() || amount <= 0)
        return src;
    const QImage blurred = gaussianBlur(src, qMax(1, radius));
    QImage a = src.convertToFormat(QImage::Format_ARGB32);
    QImage out = a;
    for (int y = 0; y < a.height(); ++y) {
        const QRgb *s  = reinterpret_cast<const QRgb *>(a.constScanLine(y));
        const QRgb *bl = reinterpret_cast<const QRgb *>(blurred.constScanLine(y));
        QRgb *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            o[x] = qRgba(
                qBound(0, int(qRed(s[x])   + amount * (qRed(s[x])   - qRed(bl[x]))   + 0.5), 255),
                qBound(0, int(qGreen(s[x]) + amount * (qGreen(s[x]) - qGreen(bl[x])) + 0.5), 255),
                qBound(0, int(qBlue(s[x])  + amount * (qBlue(s[x])  - qBlue(bl[x]))  + 0.5), 255),
                qAlpha(s[x]));
        }
    }
    return out;
}

// ============================================================================
// 几何变换
// ============================================================================

QImage rotate(const QImage &src, double degrees, const QColor &background)
{
    if (src.isNull() || std::fmod(degrees, 360.0) == 0.0)
        return src;

    QTransform t;
    t.rotate(degrees);
    const QImage rotated = src.transformed(t, Qt::SmoothTransformation);

    QImage out(rotated.size(), QImage::Format_ARGB32);
    out.fill(background.rgba());
    QPainter p(&out);
    p.drawImage(0, 0, rotated);
    p.end();
    return out;
}

QImage flip(const QImage &src, bool horizontal, bool vertical)
{
    if (src.isNull())
        return src;
    return src.mirrored(horizontal, vertical);
}

QImage crop(const QImage &src, const QRect &rectIn)
{
    if (src.isNull())
        return src;
    QRect rect = rectIn.normalized().intersected(src.rect());
    if (rect.isEmpty())
        return src;
    return src.copy(rect);
}

QImage resizeImage(const QImage &src, const QSize &newSize)
{
    if (src.isNull() || !newSize.isValid() || newSize.isEmpty())
        return src;
    return src.scaled(newSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage resizeCanvas(const QImage &src, const QSize &newSize, int anchor,
                    const QColor &background)
{
    if (src.isNull() || !newSize.isValid() || newSize.isEmpty())
        return src;

    QImage out(newSize, QImage::Format_ARGB32);
    out.fill(background.rgba());

    int x = 0, y = 0;
    const int col = anchor % 3; // 0 左 1 中 2 右
    const int row = anchor / 3; // 0 上 1 中 2 下
    if (col == 1) x = (newSize.width()  - src.width())  / 2;
    if (col == 2) x = newSize.width()  - src.width();
    if (row == 1) y = (newSize.height() - src.height()) / 2;
    if (row == 2) y = newSize.height() - src.height();

    QPainter p(&out);
    p.drawImage(qMax(0, x), qMax(0, y), src);
    p.end();
    return out;
}

// ============================================================================
// 水印消除：多尺度扩散修补 (Laplace inpainting with pyramid initialization)
// ============================================================================

namespace {

struct FloatImg
{
    int w = 0, h = 0;
    std::vector<float> r, g, b;   // w*h
    std::vector<quint8> hole;     // 1 = 待修补
};

FloatImg toFloat(const QImage &img, const QImage &mask, long *holeCount)
{
    FloatImg f;
    f.w = img.width();
    f.h = img.height();
    const size_t n = size_t(f.w) * size_t(f.h);
    f.r.resize(n); f.g.resize(n); f.b.resize(n);
    f.hole.assign(n, 0);

    QImage m = mask;
    if (m.size() != img.size())
        m = m.scaled(img.size());
    if (m.format() != QImage::Format_Grayscale8)
        m = m.convertToFormat(QImage::Format_Grayscale8);

    long holes = 0;
    for (int y = 0; y < f.h; ++y) {
        const QRgb *in = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        const quint8 *mk = m.constScanLine(y);
        for (int x = 0; x < f.w; ++x) {
            const size_t i = size_t(y) * f.w + x;
            f.r[i] = float(qRed(in[x]));
            f.g[i] = float(qGreen(in[x]));
            f.b[i] = float(qBlue(in[x]));
            if (mk[x] > 128) { f.hole[i] = 1; ++holes; }
        }
    }
    if (holeCount) *holeCount = holes;
    return f;
}

FloatImg downsample(const FloatImg &src)
{
    FloatImg d;
    d.w = qMax(1, src.w / 2);
    d.h = qMax(1, src.h / 2);
    const size_t n = size_t(d.w) * size_t(d.h);
    d.r.assign(n, 0.f); d.g.assign(n, 0.f); d.b.assign(n, 0.f);
    d.hole.assign(n, 0);

    for (int y = 0; y < d.h; ++y) {
        for (int x = 0; x < d.w; ++x) {
            int cnt = 0, holeCnt = 0;
            float rr = 0, gg = 0, bb = 0;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int sx = 2 * x + dx, sy = 2 * y + dy;
                    if (sx >= src.w || sy >= src.h) continue;
                    const size_t si = size_t(sy) * src.w + sx;
                    ++cnt;
                    if (src.hole[si]) {
                        ++holeCnt;
                    } else {
                        rr += src.r[si]; gg += src.g[si]; bb += src.b[si];
                    }
                }
            }
            const size_t di = size_t(y) * d.w + x;
            const int solid = cnt - holeCnt;
            if (solid > 0) {
                d.r[di] = rr / solid;
                d.g[di] = gg / solid;
                d.b[di] = bb / solid;
            }
            if (holeCnt > 0)
                d.hole[di] = 1;
        }
    }
    return d;
}

// Gauss-Seidel 松弛：只更新 hole 像素，非 hole 像素作为 Dirichlet 边界
void relax(FloatImg &f, int iterations)
{
    const int w = f.w, h = f.h;
    for (int it = 0; it < iterations; ++it) {
        for (int y = 0; y < h; ++y) {
            const int yUp   = qMax(y - 1, 0) * w;
            const int yDown = qMin(y + 1, h - 1) * w;
            const int yRow  = y * w;
            for (int x = 0; x < w; ++x) {
                const size_t i = size_t(yRow) + x;
                if (!f.hole[i]) continue;
                const int xl = qMax(x - 1, 0);
                const int xr = qMin(x + 1, w - 1);
                f.r[i] = 0.25f * (f.r[yRow + xl] + f.r[yRow + xr] + f.r[yUp + x] + f.r[yDown + x]);
                f.g[i] = 0.25f * (f.g[yRow + xl] + f.g[yRow + xr] + f.g[yUp + x] + f.g[yDown + x]);
                f.b[i] = 0.25f * (f.b[yRow + xl] + f.b[yRow + xr] + f.b[yUp + x] + f.b[yDown + x]);
            }
        }
    }
}

// 将粗一层的解双线性上采样，作为细一层 hole 像素的初值
void upsampleInto(const FloatImg &coarse, FloatImg &fine)
{
    for (int y = 0; y < fine.h; ++y) {
        for (int x = 0; x < fine.w; ++x) {
            const size_t i = size_t(y) * fine.w + x;
            if (!fine.hole[i]) continue;

            float cx = (x + 0.5f) * 0.5f - 0.5f;
            float cy = (y + 0.5f) * 0.5f - 0.5f;
            cx = qBound(0.f, cx, float(coarse.w - 1));
            cy = qBound(0.f, cy, float(coarse.h - 1));
            const int x0 = int(cx), y0 = int(cy);
            const int x1 = qMin(x0 + 1, coarse.w - 1);
            const int y1 = qMin(y0 + 1, coarse.h - 1);
            const float fx = cx - x0, fy = cy - y0;

            const size_t a = size_t(y0) * coarse.w + x0;
            const size_t b = size_t(y0) * coarse.w + x1;
            const size_t c = size_t(y1) * coarse.w + x0;
            const size_t d = size_t(y1) * coarse.w + x1;
            fine.r[i] = (coarse.r[a] * (1 - fx) + coarse.r[b] * fx) * (1 - fy)
                      + (coarse.r[c] * (1 - fx) + coarse.r[d] * fx) * fy;
            fine.g[i] = (coarse.g[a] * (1 - fx) + coarse.g[b] * fx) * (1 - fy)
                      + (coarse.g[c] * (1 - fx) + coarse.g[d] * fx) * fy;
            fine.b[i] = (coarse.b[a] * (1 - fx) + coarse.b[b] * fx) * (1 - fy)
                      + (coarse.b[c] * (1 - fx) + coarse.b[d] * fx) * fy;
        }
    }
}

} // namespace

QImage inpaint(const QImage &srcIn, const QImage &mask)
{
    if (srcIn.isNull() || srcIn.size() != mask.size())
        return srcIn;

    QImage out = srcIn.convertToFormat(QImage::Format_ARGB32);

    long holes = 0;
    FloatImg base = toFloat(out, mask, &holes);
    if (holes == 0)
        return srcIn;
    if (holes >= long(base.w) * base.h) {
        // 整图被遮住：直接填充中灰无意义，返回原图
        return srcIn;
    }

    // 构建金字塔
    std::vector<FloatImg> levels;
    levels.push_back(std::move(base));
    while (levels.back().w > 48 && levels.back().h > 48
           && levels.back().w / 2 >= 1 && levels.back().h / 2 >= 1) {
        levels.push_back(downsample(levels.back()));
    }

    // 最粗一层：用非 hole 像素均值初始化 hole
    FloatImg &c = levels.back();
    {
        double sr = 0, sg = 0, sb = 0;
        long cnt = 0;
        const size_t n = size_t(c.w) * c.h;
        for (size_t i = 0; i < n; ++i) {
            if (!c.hole[i]) { sr += c.r[i]; sg += c.g[i]; sb += c.b[i]; ++cnt; }
        }
        const float mr = cnt ? float(sr / cnt) : 128.f;
        const float mg = cnt ? float(sg / cnt) : 128.f;
        const float mb = cnt ? float(sb / cnt) : 128.f;
        for (size_t i = 0; i < n; ++i) {
            if (c.hole[i]) { c.r[i] = mr; c.g[i] = mg; c.b[i] = mb; }
        }
    }
    relax(c, 300);

    // 由粗到细逐层求解
    for (size_t l = levels.size() - 1; l-- > 0;) {
        upsampleInto(levels[l + 1], levels[l]);
        relax(levels[l], 120);
    }

    // 写回图像（hole 像素替换为求解结果，保留原 alpha）
    FloatImg &res = levels[0];
    for (int y = 0; y < out.height(); ++y) {
        QRgb *o = reinterpret_cast<QRgb *>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const size_t i = size_t(y) * res.w + x;
            if (!res.hole[i]) continue;
            o[x] = qRgba(qBound(0, int(res.r[i] + 0.5f), 255),
                         qBound(0, int(res.g[i] + 0.5f), 255),
                         qBound(0, int(res.b[i] + 0.5f), 255),
                         qAlpha(o[x]));
        }
    }
    return out;
}

// ============================================================================
// 绘图工具：洪水填充（油漆桶）
// ============================================================================

bool floodFill(QImage &img, int x, int y, const QColor &fillColor,
               int tolerance, bool diagonal)
{
    if (img.isNull() || x < 0 || y < 0 || x >= img.width() || y >= img.height())
        return false;

    if (img.format() != QImage::Format_ARGB32_Premultiplied)
        img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);

    const int w = img.width(), h = img.height();
    auto at = [&](int px, int py) -> QRgb {
        return *reinterpret_cast<const QRgb *>(img.constScanLine(py) + px * 4);
    };
    auto setAt = [&](int px, int py, QRgb v) {
        *reinterpret_cast<QRgb *>(img.scanLine(py) + px * 4) = v;
    };

    const QRgb target = at(x, y);
    const QRgb fill = fillColor.rgba();
    if (target == fill)
        return false;   // 颜色相同，无需填充

    const int tr = qRed(target), tg = qGreen(target), tb = qBlue(target), ta = qAlpha(target);
    auto matches = [&](QRgb px) {
        const int d = qAbs(qRed(px) - tr) + qAbs(qGreen(px) - tg)
                    + qAbs(qBlue(px) - tb) + qAbs(qAlpha(px) - ta);
        return d <= tolerance * 4;   // 每通道平均容差
    };
    if (!matches(target))
        return false;

    // 扫描线/栈式填充，用 visited 位图防止「填充色仍匹配目标」时无限循环
    std::vector<quint8> visited(size_t(w) * size_t(h), 0);
    QVector<int> stack;
    stack.reserve(1024);
    auto pushIf = [&](int px, int py) {
        if (px < 0 || py < 0 || px >= w || py >= h)
            return;
        const int idx = py * w + px;
        if (visited[idx])
            return;
        if (!matches(at(px, py)))
            return;
        visited[idx] = 1;
        stack.push_back(idx);
    };
    visited[size_t(y) * w + x] = 1;
    stack.push_back(y * w + x);

    const int dx[4] = { -1, 1, 0, 0 };
    const int dy[4] = { 0, 0, -1, 1 };
    const int ddx[4] = { -1, 1, -1, 1 };
    const int ddy[4] = { -1, -1, 1, 1 };

    while (!stack.isEmpty()) {
        const int idx = stack.back();
        stack.pop_back();
        const int px = idx % w, py = idx / w;
        setAt(px, py, fill);
        for (int k = 0; k < 4; ++k)
            pushIf(px + dx[k], py + dy[k]);
        if (diagonal)
            for (int k = 0; k < 4; ++k)
                pushIf(px + ddx[k], py + ddy[k]);
    }
    return true;
}

} // namespace ImageOps
