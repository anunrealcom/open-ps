#pragma once

#include <QWidget>

class QLabel;
class QToolButton;

// ============================================================================
// TitleBar: 无边框窗口的自定义标题栏。
// 左侧 logo + 标题，右侧 最小化/最大化/关闭 按钮。
// 空白处按下可拖动窗口（startSystemMove），双击切换最大化。
// ============================================================================
class TitleBar : public QWidget
{
    Q_OBJECT
public:
    explicit TitleBar(QWidget *parent = nullptr);

    void setMaximizedState(bool maximized); // 切换最大化/还原图标
    void setTitleText(const QString &text); // 更新标题文字（文件名）

signals:
    void minimizeRequested();
    void maximizeToggleRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    QToolButton *makeButton(const QString &iconName, const char *objName);

    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QToolButton *m_minBtn = nullptr;
    QToolButton *m_maxBtn = nullptr;
    QToolButton *m_closeBtn = nullptr;
    bool m_maximized = false;
};
