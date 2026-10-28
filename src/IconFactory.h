#pragma once

#include <QIcon>
#include <QColor>
#include <QString>
#include <QPixmap>

// ============================================================================
// IconFactory: 用 QPainter 矢量绘制的扁平风格图标集合。
// 不依赖任何外部图片资源，分辨率无关，随主题色变化。
// 名称见 icon() 的 switch 列表。
// ============================================================================
namespace IconFactory {

// 返回指定名称的图标；color 为线条/主色（默认使用 QQ 蓝）。
QIcon icon(const QString &name, const QColor &color = QColor("#12B7F5"));

// 单独绘制某个图标为 QPixmap（供标题栏按钮等复用）。
// size 为逻辑像素，内部按 2x 绘制保证清晰。
QPixmap pixmap(const QString &name, int size, const QColor &color);

} // namespace IconFactory
