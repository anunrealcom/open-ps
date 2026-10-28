#pragma once

#include <QDialog>
#include <QImage>
#include <QColor>
#include <QFont>

#include "ImageOps.h"

class QLabel;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QToolButton;
class QTimer;
class QVBoxLayout;
class QPlainTextEdit;

// ============================================================================
// PreviewDialog: 带实时预览的对话框基类。
// 预览基于缩放后的图（最长边 <= 900px）以保证滑块拖动时足够流畅；
// 确认后在 MainWindow 中对原图调用 applyTo() 应用同样参数。
// ============================================================================
class PreviewDialog : public QDialog
{
    Q_OBJECT
public:
    PreviewDialog(QWidget *parent, const QImage &image, const QString &title);

    // 对任意图像应用当前参数（用于确认时处理原图）
    QImage applyTo(const QImage &src) const;

protected:
    // 子类实现：根据当前控件状态生成结果图
    virtual QImage generate(const QImage &src) const = 0;
    // 子类实现：恢复默认参数
    virtual void resetValues() = 0;

    void refreshPreview();          // 防抖刷新预览
    void addOptions(QLayout *layout);

private:
    void showPreviewNow();

    QImage m_source;   // 原图
    QImage m_base;     // 缩放后的预览底图
    QLabel *m_previewLabel = nullptr;
    QTimer *m_debounce = nullptr;
    QVBoxLayout *m_optionsLayout = nullptr;
};

// ---- 亮度/对比度/饱和度/色温 ------------------------------------------------
class AdjustDialog : public PreviewDialog
{
    Q_OBJECT
public:
    AdjustDialog(QWidget *parent, const QImage &image);
    ImageOps::AdjustParams params() const;

protected:
    QImage generate(const QImage &src) const override;
    void resetValues() override;

private:
    QWidget *buildRow(const QString &name, QSlider *slider, QLabel *valueLabel,
                      int min, int max) const;

    QSlider *m_brightness = nullptr;
    QSlider *m_contrast = nullptr;
    QSlider *m_saturation = nullptr;
    QSlider *m_temperature = nullptr;
    QLabel *m_brightVal = nullptr;
    QLabel *m_contrastVal = nullptr;
    QLabel *m_satVal = nullptr;
    QLabel *m_tempVal = nullptr;
};

// ---- 高斯模糊 ----------------------------------------------------------------
class BlurDialog : public PreviewDialog
{
    Q_OBJECT
public:
    BlurDialog(QWidget *parent, const QImage &image);
    int radius() const;

protected:
    QImage generate(const QImage &src) const override;
    void resetValues() override;

private:
    QSlider *m_radius = nullptr;
    QLabel *m_radiusVal = nullptr;
};

// ---- 锐化 --------------------------------------------------------------------
class SharpenDialog : public PreviewDialog
{
    Q_OBJECT
public:
    SharpenDialog(QWidget *parent, const QImage &image);
    int amountPercent() const;

protected:
    QImage generate(const QImage &src) const override;
    void resetValues() override;

private:
    QSlider *m_amount = nullptr;
    QLabel *m_amountVal = nullptr;
};

// ---- 任意角度旋转 --------------------------------------------------------------
class RotateDialog : public PreviewDialog
{
    Q_OBJECT
public:
    RotateDialog(QWidget *parent, const QImage &image);
    double angle() const;
    QColor background() const { return m_bg; }

protected:
    QImage generate(const QImage &src) const override;
    void resetValues() override;

private slots:
    void pickColor();

private:
    QSlider *m_angle = nullptr;
    QSpinBox *m_angleSpin = nullptr;
    QColor m_bg = Qt::white;
    QPushButton *m_bgButton = nullptr;
};

// ---- 调整图像大小（无预览，简单对话框）-------------------------------------------
class ResizeDialog : public QDialog
{
    Q_OBJECT
public:
    ResizeDialog(QWidget *parent, const QSize &currentSize);
    QSize targetSize() const;
private slots:
    void onWidthChanged(int w);
    void onHeightChanged(int h);
private:
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    QCheckBox *m_keepAspect = nullptr;
    QSize m_original;
};

// ---- 调整画布大小 ---------------------------------------------------------------
class CanvasResizeDialog : public QDialog
{
    Q_OBJECT
public:
    CanvasResizeDialog(QWidget *parent, const QSize &currentSize);
    QSize targetSize() const;
    int anchor() const { return m_anchor; }
    QColor background() const { return m_bg; }
private slots:
    void pickColor();
private:
    QSpinBox *m_width = nullptr;
    QSpinBox *m_height = nullptr;
    int m_anchor = 4; // 默认居中
    QColor m_bg = Qt::white;
    QPushButton *m_bgButton = nullptr;
};

// ---- 文字工具 -------------------------------------------------------------------
class TextDialog : public QDialog
{
    Q_OBJECT
public:
    TextDialog(QWidget *parent);
    QString text() const;
    QFont font() const { return m_font; }
    QColor color() const { return m_color; }
private slots:
    void pickFont();
    void pickColor();
private:
    QPlainTextEdit *m_edit = nullptr;
    QFont m_font;
    QColor m_color = Qt::red;
    QPushButton *m_fontButton = nullptr;
    QPushButton *m_colorButton = nullptr;
};
