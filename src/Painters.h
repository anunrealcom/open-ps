#pragma once

#include <QImage>
#include <QString>
#include <QJsonObject>

// ============================================================================
// Painters: 内置程序化自动作画（无需联网/外部模型）。
// 所有函数就地绘制到传入的图层 image（ARGB32_Premultiplied），带 seed 可复现。
// ============================================================================
namespace Painters {

// 统一入口：kind = plasma | mandelbrot | julia | shapes | flowfield | gradient
// 返回是否成功。params 依 kind 可选：seed, scale, count, palette, angle, cx, cy ...
bool generate(QImage &image, const QString &kind, const QJsonObject &params);

// 单项接口（供 AI 接口或测试直接调用）
void plasma(QImage &image, quint32 seed, double scale = 0.02);
void mandelbrot(QImage &image, quint32 seed, double zoom = 1.0);
void julia(QImage &image, quint32 seed, double cx = -0.8, double cy = 0.156);
void randomShapes(QImage &image, quint32 seed, int count = 60);
void flowField(QImage &image, quint32 seed, int particles = 800, int steps = 120);
void gradient(QImage &image, quint32 seed, bool radial = false);

} // namespace Painters
