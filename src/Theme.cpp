#include "Theme.h"

#include <QPixmap>

namespace Theme {

QPixmap logo(int size)
{
    QPixmap pm(":/logo.png");
    if (pm.isNull())
        return QPixmap();
    return pm.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QString styleSheet(int radius)
{
    const int r = radius;
    return QString(R"(
/* ---------- 根容器：圆角白色面板 ---------- */
#RootFrame {
    background: %1;
    border: 1px solid %2;
    border-radius: %3px;
}

/* ---------- 通用文字 ---------- */
QWidget {
    color: %4;
    font-family: "Microsoft YaHei UI", "Microsoft YaHei", "Segoe UI", sans-serif;
    font-size: 13px;
}

/* ---------- 标题栏 ---------- */
#TitleBar {
    background: transparent;
    border-top-left-radius: %3px;
    border-top-right-radius: %3px;
}
#AppTitle {
    color: %4;
    font-size: 13px;
    font-weight: bold;
    background: transparent;
}
#TitleBtn {
    border: none;
    background: transparent;
    border-radius: 6px;
}
#TitleBtn:hover { background: #ECEFf2; }
#TitleBtn:pressed { background: #DDE1E6; }
#CloseBtn:hover { background: #E81123; }
#CloseBtn:hover:pressed { background: #C50F1F; }

/* ---------- 菜单栏 ---------- */
QMenuBar {
    background: transparent;
    border: none;
    padding: 2px 6px;
    spacing: 2px;
}
QMenuBar::item {
    background: transparent;
    padding: 6px 12px;
    margin: 1px 0;
    border-radius: 6px;
    color: %4;
}
QMenuBar::item:selected {
    background: %5;
    color: %6;
}

/* ---------- 下拉菜单 ---------- */
QMenu {
    background: #FFFFFF;
    border: 1px solid %2;
    border-radius: 10px;
    padding: 6px;
}
QMenu::item {
    padding: 6px 30px 6px 12px;
    border-radius: 6px;
    color: %4;
}
QMenu::item:selected {
    background: %6;
    color: white;
}
QMenu::icon { padding-left: 4px; }
QMenu::separator {
    height: 1px;
    background: %2;
    margin: 6px 8px;
}
QMenu::right-arrow { margin-right: 6px; }

/* ---------- 工具栏 ---------- */
QToolBar {
    background: %7;
    border: none;
    border-bottom: 1px solid %2;
    padding: 5px 8px;
    spacing: 4px;
}
QToolBar::separator {
    background: %2;
    width: 1px;
    margin: 4px 6px;
}
QToolButton {
    background: transparent;
    border: none;
    border-radius: 8px;
    padding: 6px 10px;
    color: %4;
}
QToolButton:hover { background: #E9F6FD; }
QToolButton:pressed { background: #D6EEFA; }
QToolButton:checked { background: %6; color: white; }

/* ---------- 状态栏 ---------- */
QStatusBar {
    background: %7;
    border-top: 1px solid %2;
    border-bottom-left-radius: %3px;
    border-bottom-right-radius: %3px;
    color: %8;
}
QStatusBar::item { border: none; }
QStatusBar QLabel { color: %8; background: transparent; }

/* ---------- 滑块 ---------- */
QSlider::groove:horizontal {
    height: 4px; background: #D7DCE2; border-radius: 2px;
}
QSlider::sub-page:horizontal {
    height: 4px; background: %6; border-radius: 2px;
}
QSlider::handle:horizontal {
    background: white; border: 2px solid %6;
    width: 12px; height: 12px; margin: -6px 0; border-radius: 8px;
}
QSlider::handle:horizontal:hover { border: 2px solid %9; }

/* ---------- 对话框按钮 ---------- */
QPushButton {
    background: #F0F2F5; color: %4;
    border: 1px solid %2; border-radius: 8px;
    padding: 6px 16px;
}
QPushButton:hover { background: #E6E9EE; }
QPushButton:pressed { background: #DCE0E6; }
QPushButton:default {
    background: %6; color: white; border: none;
}
QPushButton:default:hover { background: %9; }

/* ---------- 输入控件 ---------- */
QSpinBox, QDoubleSpinBox, QPlainTextEdit, QLineEdit {
    background: white; border: 1px solid %2;
    border-radius: 6px; padding: 4px 6px; color: %4;
    selection-background-color: %6; selection-color: white;
}
QSpinBox:focus, QPlainTextEdit:focus, QLineEdit:focus { border: 1px solid %6; }
QCheckBox { spacing: 8px; }
QCheckBox::indicator {
    width: 16px; height: 16px; border-radius: 4px;
    border: 1px solid #C2C8D0; background: white;
}
QCheckBox::indicator:checked { background: %6; border: 1px solid %6; }

/* ---------- 滚动条 ---------- */
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #C7CDD4; border-radius: 4px; min-height: 24px; }
QScrollBar::handle:vertical:hover { background: #AEB6BF; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #C7CDD4; border-radius: 4px; min-width: 24px; }
QScrollBar::handle:horizontal:hover { background: #AEB6BF; }

/* ---------- 提示气泡 ---------- */
QToolTip {
    background: #2B2E33; color: white;
    border: none; border-radius: 6px; padding: 5px 8px;
}
)")
    .arg(Theme::windowBg().name())        // %1
    .arg(Theme::borderClr().name())       // %2
    .arg(r)                               // %3
    .arg(Theme::textPrimary().name())     // %4
    .arg(Theme::accentLight().name())     // %5
    .arg(Theme::accent().name())          // %6
    .arg(Theme::panelBg().name())         // %7
    .arg(Theme::textSecond().name())      // %8
    .arg(Theme::accentDark().name());     // %9
}

} // namespace Theme
