#pragma once

#include <QImage>
#include <QColor>
#include <QSize>
#include <QRect>

// ============================================================================
// ImageOps: 纯图像处理算法集合，不依赖任何 UI 组件
// 所有函数均为无副作用函数：输入原图，返回新图
// ============================================================================
namespace ImageOps {

// ---- 色彩调整 ---------------------------------------------------------------

struct AdjustParams
{
    int brightness = 0;  // -100..100
    int contrast   = 0;  // -100..100
    int saturation = 0;  // -100..100
    int temperature = 0; // -100..100 (负数偏冷，正数偏暖)
};

// 亮度/对比度/饱和度/色温 单次遍历合并应用
QImage applyAdjustments(const QImage &src, const AdjustParams &p);

QImage toGrayscale(const QImage &src);
QImage invertColors(const QImage &src);
QImage autoLevels(const QImage &src); // 自动色阶（直方图 0.25% 裁剪拉伸）

// ---- 滤镜 -------------------------------------------------------------------

// 高斯模糊，radius >= 1
QImage gaussianBlur(const QImage &src, int radius);

// 锐化（unsharp mask），amount 通常 0.5 ~ 3.0
QImage sharpen(const QImage &src, int radius, double amount);

// ---- 几何变换 -----------------------------------------------------------------

// 任意角度旋转（顺时针为正），背景色填充
QImage rotate(const QImage &src, double degrees, const QColor &background);

QImage flip(const QImage &src, bool horizontal, bool vertical);

QImage crop(const QImage &src, const QRect &rect);

// 精确缩放到指定尺寸
QImage resizeImage(const QImage &src, const QSize &newSize);

// 调整画布大小；anchor 为九宫格 0..8（0=左上，4=居中，8=右下）
QImage resizeCanvas(const QImage &src, const QSize &newSize, int anchor,
                    const QColor &background);

// ---- 水印消除（内容感知修补）---------------------------------------------------

// mask: Grayscale8 格式，与 src 尺寸一致，值 > 128 的区域为待消除区域。
// 算法：多尺度（金字塔）+ Gauss-Seidel 扩散求解 Laplace 方程，
//       以未遮挡像素为边界条件，对遮挡区域做平滑内容感知填充。
QImage inpaint(const QImage &src, const QImage &mask);

// ---- 绘图工具算法 -------------------------------------------------------------

// 洪水填充（油漆桶）。就地修改 img（建议 ARGB32_Premultiplied）。
// tolerance: 与起始颜色的最大允许色差(0..255)。diagonal: 是否 8 邻域。
// 返回是否有实际填充（起点在图内）。
bool floodFill(QImage &img, int x, int y, const QColor &fillColor,
               int tolerance = 0, bool diagonal = false);

} // namespace ImageOps
