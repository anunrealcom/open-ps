#pragma once

#include <QWidget>
#include "ImageCanvas.h"

class QButtonGroup;

// ============================================================================
// ToolBox: 左侧竖向工具箱。每个工具显示 图标 + 中文，选中态高亮。
// ============================================================================
class ToolBox : public QWidget
{
    Q_OBJECT
public:
    explicit ToolBox(QWidget *parent = nullptr);

    void setTool(ImageCanvas::Tool tool);   // 同步选中态（不触发信号）

signals:
    void toolChosen(ImageCanvas::Tool tool);

private:
    QButtonGroup *m_group = nullptr;
};
