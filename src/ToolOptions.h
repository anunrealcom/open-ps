#pragma once

#include <QWidget>
#include <QColor>

class QPushButton;
class QSlider;
class QLabel;

// ============================================================================
// ToolOptions: 画笔选项面板。前景色 + 常用色板 + 画笔/橡皮大小 + 导入图片。
// ============================================================================
class ToolOptions : public QWidget
{
    Q_OBJECT
public:
    explicit ToolOptions(QWidget *parent = nullptr);

    void setColor(const QColor &c);      // 外部（如取色器）更新前景色显示
    void setSize(int s);

signals:
    void colorChosen(const QColor &color);
    void sizeChanged(int size);
    void importRequested();

private:
    void rebuildSwatch();

    QPushButton *m_swatch = nullptr;
    QLabel *m_sizeVal = nullptr;
    QSlider *m_size = nullptr;
    QColor m_color = Qt::black;
};
