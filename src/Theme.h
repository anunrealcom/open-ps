#pragma once

#include <QColor>
#include <QString>
#include <QPixmap>

// ============================================================================
// Theme: QQ 风格的调色板与全局样式表。
// ============================================================================
namespace Theme {

// 主色（QQ 蓝）
inline QColor accent()      { return QColor("#12B7F5"); }
inline QColor accentDark()  { return QColor("#0FA0D6"); }
inline QColor accentLight() { return QColor("#E5F6FE"); }
inline QColor textPrimary() { return QColor("#17181A"); }
inline QColor textSecond()  { return QColor("#8A9099"); }
inline QColor windowBg()    { return QColor("#FFFFFF"); }
inline QColor panelBg()     { return QColor("#F5F7FA"); }
inline QColor borderClr()   { return QColor("#E6E9EE"); }

// 全局样式表字符串（应用到 QApplication）。
// radius 为根圆角半径，maximized 时传 0。
QString styleSheet(int radius = 14);

// 应用 logo（来自 :/logo.png），size 为逻辑像素。
QPixmap logo(int size);

} // namespace Theme
