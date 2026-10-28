#include "Document.h"
#include "ImageOps.h"

#include <QPainter>
#include <algorithm>

namespace {

// 将 src 图层按混合模式与不透明度合成到 dst（均为 ARGB32_Premultiplied，同尺寸）。
// 采用带 alpha 的标准混合公式：
//   premult_out = (1-ad)*srcP + (1-as)*dstP + as*ad*B(Cb,Cs)
//   out_alpha   = as + ad*(1-as)
// 其中 as/ad 为源/目标 alpha(0..1)，Cs/Cb 为还原后的直接色(0..255)，B 为混合函数。
void blendLayer(QImage &dst, const QImage &src, int opacity, Blend mode)
{
    const int w = std::min(dst.width(), src.width());
    const int h = std::min(dst.height(), src.height());
    const double f = opacity / 255.0;   // 图层不透明度

    for (int y = 0; y < h; ++y) {
        QRgb *d = reinterpret_cast<QRgb *>(dst.scanLine(y));
        const QRgb *s = reinterpret_cast<const QRgb *>(src.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            const QRgb S = s[x];
            const int sa0 = qAlpha(S);
            if (sa0 == 0 || f <= 0.0)
                continue;                       // 源透明：保持不变
            const double as = (sa0 / 255.0) * f;

            const QRgb D = d[x];
            const int da0 = qAlpha(D);
            const double ad = da0 / 255.0;

            // 源直接色(不受不透明度影响)：Cs = srcP*255/sa0
            const double CsR = qRed(S)   * 255.0 / sa0;
            const double CsG = qGreen(S) * 255.0 / sa0;
            const double CsB = qBlue(S)  * 255.0 / sa0;
            // 目标直接色
            const double CbR = da0 > 0 ? qRed(D)   * 255.0 / da0 : 0.0;
            const double CbG = da0 > 0 ? qGreen(D) * 255.0 / da0 : 0.0;
            const double CbB = da0 > 0 ? qBlue(D)  * 255.0 / da0 : 0.0;

            auto blendCh = [&](double cb, double cs) {
                switch (mode) {
                case Blend::Multiply: return cb * cs / 255.0;
                case Blend::Screen:   return 255.0 - (255.0 - cb) * (255.0 - cs) / 255.0;
                case Blend::Normal:   break;
                }
                return cs;   // Normal：直接色即源色
            };
            const double BR = blendCh(CbR, CsR);
            const double BG = blendCh(CbG, CsG);
            const double BB = blendCh(CbB, CsB);

            // 源预乘色(乘不透明度)
            const double spR = qRed(S)   * f;
            const double spG = qGreen(S) * f;
            const double spB = qBlue(S)  * f;

            const double ao = as + ad * (1.0 - as);
            if (ao <= 0.0) { d[x] = 0; continue; }

            const double outR = (1.0 - ad) * spR + (1.0 - as) * qRed(D)   + as * ad * BR;
            const double outG = (1.0 - ad) * spG + (1.0 - as) * qGreen(D) + as * ad * BG;
            const double outB = (1.0 - ad) * spB + (1.0 - as) * qBlue(D)  + as * ad * BB;

            d[x] = qRgba(qBound(0, int(outR + 0.5), 255),
                         qBound(0, int(outG + 0.5), 255),
                         qBound(0, int(outB + 0.5), 255),
                         qBound(0, int(ao * 255.0 + 0.5), 255));
        }
    }
}

} // namespace

Document::Document(int w, int h, const QString &baseName)
    : m_width(qMax(1, w)), m_height(qMax(1, h))
{
    Layer l;
    l.name = baseName;
    l.image = QImage(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::transparent);
    m_layers.push_back(l);
    m_active = 0;
}

Document Document::fromImage(const QImage &img, const QString &name)
{
    Document d(img.width(), img.height(), name);
    d.m_layers[0].image = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    return d;
}

// ---------------------------------------------------------------------------
// 图层访问
// ---------------------------------------------------------------------------

void Document::setActiveIndex(int i)
{
    if (i >= 0 && i < m_layers.size())
        m_active = i;
}

Layer *Document::activeLayer()
{
    if (m_layers.isEmpty())
        return nullptr;
    return &m_layers[m_active];
}

const Layer *Document::activeLayer() const
{
    if (m_layers.isEmpty())
        return nullptr;
    return &m_layers[m_active];
}

QString Document::activeLayerName() const
{
    return m_layers.isEmpty() ? QString() : m_layers[m_active].name;
}

void Document::clampActive()
{
    if (m_active >= m_layers.size())
        m_active = m_layers.size() - 1;
    if (m_active < 0)
        m_active = 0;
}

// ---------------------------------------------------------------------------
// 合成
// ---------------------------------------------------------------------------

QImage Document::composite() const
{
    QImage out(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    // 是否存在非正常混合模式的可见图层
    bool anySpecial = false;
    for (const Layer &l : m_layers)
        if (l.visible && l.opacity > 0 && l.blend != Blend::Normal) { anySpecial = true; break; }

    if (!anySpecial) {
        // 快路径：全部正常模式，用 QPainter
        QPainter p(&out);
        p.setCompositionMode(QPainter::CompositionMode_SourceOver);
        for (const Layer &l : m_layers) {
            if (!l.visible || l.opacity <= 0 || l.image.isNull())
                continue;
            p.setOpacity(l.opacity / 255.0);
            p.drawImage(0, 0, l.image);
        }
        p.end();
        return out;
    }

    // 慢路径：逐像素按混合模式合成
    for (const Layer &l : m_layers) {
        if (!l.visible || l.opacity <= 0 || l.image.isNull())
            continue;
        blendLayer(out, l.image, l.opacity, l.blend);
    }
    return out;
}

// ---------------------------------------------------------------------------
// 结构操作
// ---------------------------------------------------------------------------

void Document::addLayer(const QString &name, const QColor &fill)
{
    Layer l;
    l.name = name.isEmpty() ? QStringLiteral("图层 %1").arg(m_layers.size() + 1) : name;
    l.image = QImage(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    l.image.fill(fill.isValid() && fill.alpha() > 0 ? fill : Qt::transparent);
    m_layers.push_back(l);
    m_active = m_layers.size() - 1;   // 新图层置顶并设为活动
}

void Document::addImageLayer(const QImage &img, const QString &name)
{
    if (img.isNull())
        return;
    Layer l;
    l.name = name.isEmpty() ? QStringLiteral("图片 %1").arg(m_layers.size() + 1) : name;
    l.image = QImage(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::transparent);
    QImage src = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    // 过大则等比缩放到文档内
    if (src.width() > m_width || src.height() > m_height)
        src = src.scaled(m_width, m_height, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int x = (m_width - src.width()) / 2;
    const int y = (m_height - src.height()) / 2;
    QPainter p(&l.image);
    p.drawImage(x, y, src);
    p.end();
    m_layers.push_back(l);
    m_active = m_layers.size() - 1;
}

void Document::insertLayerAboveActive(const QString &name)
{
    Layer l;
    l.name = name.isEmpty() ? QStringLiteral("图层 %1").arg(m_layers.size() + 1) : name;
    l.image = QImage(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
    l.image.fill(Qt::transparent);
    m_layers.insert(m_active + 1, l);
    m_active = m_active + 1;
}

void Document::removeLayer(int i)
{
    if (i < 0 || i >= m_layers.size() || m_layers.size() <= 1)
        return;   // 至少保留一个图层
    m_layers.removeAt(i);
    clampActive();
}

void Document::duplicateLayer(int i)
{
    if (i < 0 || i >= m_layers.size())
        return;
    Layer copy = m_layers[i];
    copy.name = m_layers[i].name + QStringLiteral(" 副本");
    copy.image = m_layers[i].image.copy();   // 深拷贝，独立可编辑
    m_layers.insert(i + 1, copy);
    m_active = i + 1;
}

void Document::moveLayer(int from, int to)
{
    if (from < 0 || from >= m_layers.size() || to < 0 || to >= m_layers.size() || from == to)
        return;
    Layer l = m_layers.takeAt(from);
    m_layers.insert(to, l);
    m_active = to;
}

void Document::setLayerVisible(int i, bool v)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].visible = v;
}

void Document::setLayerOpacity(int i, int opacity)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].opacity = qBound(0, opacity, 255);
}

void Document::setLayerBlend(int i, Blend b)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].blend = b;
}

void Document::renameLayer(int i, const QString &name)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].name = name;
}

void Document::clearLayer(int i)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].image.fill(Qt::transparent);
}

void Document::clearLayerTo(int i, const QColor &color)
{
    if (i >= 0 && i < m_layers.size())
        m_layers[i].image.fill(color);
}

// ---------------------------------------------------------------------------
// 几何操作（作用于所有图层）
// ---------------------------------------------------------------------------

void Document::resizeAll(const QSize &newSize)
{
    if (newSize.isEmpty())
        return;
    for (Layer &l : m_layers)
        l.image = ImageOps::resizeImage(l.image, newSize);
    m_width = newSize.width();
    m_height = newSize.height();
}

void Document::cropAll(const QRect &rect)
{
    const QRect r = rect.normalized().intersected(QRect(0, 0, m_width, m_height));
    if (r.width() < 1 || r.height() < 1)
        return;
    for (Layer &l : m_layers)
        l.image = l.image.copy(r);
    m_width = r.width();
    m_height = r.height();
}

void Document::rotateAll90(bool clockwise)
{
    const double deg = clockwise ? 90.0 : -90.0;
    for (Layer &l : m_layers)
        l.image = ImageOps::rotate(l.image, deg, Qt::transparent);
    std::swap(m_width, m_height);
}

void Document::rotateAll(double degrees, const QColor &bg)
{
    if (m_layers.isEmpty())
        return;
    QSize newSize;
    for (Layer &l : m_layers) {
        l.image = ImageOps::rotate(l.image, degrees, bg);
        newSize = l.image.size();
    }
    if (newSize.isValid() && !newSize.isEmpty()) {
        m_width = newSize.width();
        m_height = newSize.height();
    }
}

void Document::flipAll(bool horizontal, bool vertical)
{
    for (Layer &l : m_layers)
        l.image = ImageOps::flip(l.image, horizontal, vertical);
}

void Document::resizeCanvasAll(const QSize &newSize, int anchor, const QColor &bg)
{
    if (newSize.isEmpty())
        return;
    for (Layer &l : m_layers)
        l.image = ImageOps::resizeCanvas(l.image, newSize, anchor, bg);
    m_width = newSize.width();
    m_height = newSize.height();
}
