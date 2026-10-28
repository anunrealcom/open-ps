#include "dialogs.h"

#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QToolButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QPlainTextEdit>
#include <QFontDialog>
#include <QColorDialog>

// ============================================================================
// PreviewDialog
// ============================================================================

PreviewDialog::PreviewDialog(QWidget *parent, const QImage &image, const QString &title)
    : QDialog(parent)
    , m_source(image)
{
    setWindowTitle(title);
    setModal(true);
    resize(860, 640);

    constexpr int kMaxW = 900, kMaxH = 620;
    if (m_source.width() > kMaxW || m_source.height() > kMaxH)
        m_base = m_source.scaled(kMaxW, kMaxH, Qt::KeepAspectRatio,
                                 Qt::SmoothTransformation);
    else
        m_base = m_source;

    m_previewLabel = new QLabel;
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(520, 400);
    m_previewLabel->setStyleSheet(
        "background:#252528; border:1px solid #3a3a3e; border-radius:4px;");

    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(btns->button(QDialogButtonBox::Reset), &QPushButton::clicked, this, [this] {
        resetValues();
        refreshPreview();
    });

    m_debounce = new QTimer(this);
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(60);
    connect(m_debounce, &QTimer::timeout, this, &PreviewDialog::showPreviewNow);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_previewLabel, 1);
    m_optionsLayout = new QVBoxLayout;
    lay->addLayout(m_optionsLayout);
    lay->addWidget(btns);

    // 不能在基类构造函数里直接调用虚函数 generate()（此时派生类成员尚未初始化，
    // 且纯虚函数调用会导致进程 abort）。延迟到事件循环，构造完成后再刷新首帧预览。
    QTimer::singleShot(0, this, [this] { showPreviewNow(); });
}

QImage PreviewDialog::applyTo(const QImage &src) const
{
    return generate(src);
}

void PreviewDialog::refreshPreview()
{
    m_debounce->start();
}

void PreviewDialog::addOptions(QLayout *layout)
{
    m_optionsLayout->addLayout(layout);
}

void PreviewDialog::showPreviewNow()
{
    const QImage out = generate(m_base);
    if (out.isNull()) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(tr("无法生成预览"));
        return;
    }
    const QSize avail = m_previewLabel->size() - QSize(12, 12);
    QPixmap pm = QPixmap::fromImage(out).scaled(
        avail.width() > 0 ? avail.width() : 1,
        avail.height() > 0 ? avail.height() : 1,
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewLabel->setPixmap(pm);
}

// ============================================================================
// AdjustDialog
// ============================================================================

namespace {

void updateColorButton(QPushButton *btn, const QColor &c)
{
    // 用纯 Qt API 设置按钮背景色，兼容浅色/深色主题
    btn->setStyleSheet(QString("QPushButton { background-color: %1; color: %2; "
                               "padding: 4px 10px; }")
                           .arg(c.name(QColor::HexRgb),
                                c.lightness() > 128 ? "#000000" : "#ffffff"));
}

QWidget *makeSliderRow(const QString &name, QSlider *slider, QLabel *valueLabel,
                       int min, int max)
{
    slider->setRange(min, max);
    slider->setOrientation(Qt::Horizontal);
    auto *row = new QWidget;
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *nameLab = new QLabel(name);
    nameLab->setMinimumWidth(72);
    valueLabel->setMinimumWidth(48);
    valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lay->addWidget(nameLab);
    lay->addWidget(slider, 1);
    lay->addWidget(valueLabel);
    return row;
}

} // namespace

AdjustDialog::AdjustDialog(QWidget *parent, const QImage &image)
    : PreviewDialog(parent, image, tr("亮度 / 对比度 / 饱和度 / 色温"))
{
    m_brightness = new QSlider;
    m_contrast = new QSlider;
    m_saturation = new QSlider;
    m_temperature = new QSlider;
    m_brightVal = new QLabel;
    m_contrastVal = new QLabel;
    m_satVal = new QLabel;
    m_tempVal = new QLabel;

    auto sync = [this](QSlider *s, QLabel *l) {
        l->setNum(s->value());
    };

    auto *box = new QWidget;
    auto *lay = new QVBoxLayout(box);
    lay->setContentsMargins(4, 8, 4, 0);
    lay->addWidget(makeSliderRow(tr("亮度"), m_brightness, m_brightVal, -100, 100));
    lay->addWidget(makeSliderRow(tr("对比度"), m_contrast, m_contrastVal, -100, 100));
    lay->addWidget(makeSliderRow(tr("饱和度"), m_saturation, m_satVal, -100, 100));
    lay->addWidget(makeSliderRow(tr("色温"), m_temperature, m_tempVal, -100, 100));

    for (QSlider *s : { m_brightness, m_contrast, m_saturation, m_temperature }) {
        connect(s, &QSlider::valueChanged, this, [this, sync](int) {
            sync(m_brightness, m_brightVal);
            sync(m_contrast, m_contrastVal);
            sync(m_saturation, m_satVal);
            sync(m_temperature, m_tempVal);
            refreshPreview();
        });
    }

    auto *outer = new QVBoxLayout;
    outer->addWidget(box);
    addOptions(outer);
}

ImageOps::AdjustParams AdjustDialog::params() const
{
    ImageOps::AdjustParams p;
    p.brightness = m_brightness->value();
    p.contrast = m_contrast->value();
    p.saturation = m_saturation->value();
    p.temperature = m_temperature->value();
    return p;
}

QImage AdjustDialog::generate(const QImage &src) const
{
    return ImageOps::applyAdjustments(src, params());
}

void AdjustDialog::resetValues()
{
    for (QSlider *s : { m_brightness, m_contrast, m_saturation, m_temperature })
        s->setValue(0);
}

// ============================================================================
// BlurDialog
// ============================================================================

BlurDialog::BlurDialog(QWidget *parent, const QImage &image)
    : PreviewDialog(parent, image, tr("高斯模糊"))
{
    m_radius = new QSlider;
    m_radiusVal = new QLabel;
    auto *row = makeSliderRow(tr("模糊半径"), m_radius, m_radiusVal, 1, 25);
    m_radius->setValue(3);

    connect(m_radius, &QSlider::valueChanged, this, [this](int v) {
        m_radiusVal->setNum(v);
        refreshPreview();
    });

    auto *lay = new QVBoxLayout;
    lay->addWidget(row);
    addOptions(lay);
}

int BlurDialog::radius() const { return m_radius->value(); }

QImage BlurDialog::generate(const QImage &src) const
{
    return ImageOps::gaussianBlur(src, radius());
}

void BlurDialog::resetValues() { m_radius->setValue(3); }

// ============================================================================
// SharpenDialog
// ============================================================================

SharpenDialog::SharpenDialog(QWidget *parent, const QImage &image)
    : PreviewDialog(parent, image, tr("锐化"))
{
    m_amount = new QSlider;
    m_amountVal = new QLabel;
    auto *row = makeSliderRow(tr("锐化强度 %"), m_amount, m_amountVal, 10, 300);
    m_amount->setValue(100);

    connect(m_amount, &QSlider::valueChanged, this, [this](int v) {
        m_amountVal->setNum(v);
        refreshPreview();
    });

    auto *lay = new QVBoxLayout;
    lay->addWidget(row);
    addOptions(lay);
}

int SharpenDialog::amountPercent() const { return m_amount->value(); }

QImage SharpenDialog::generate(const QImage &src) const
{
    return ImageOps::sharpen(src, 2, amountPercent() / 100.0);
}

void SharpenDialog::resetValues() { m_amount->setValue(100); }

// ============================================================================
// RotateDialog
// ============================================================================

RotateDialog::RotateDialog(QWidget *parent, const QImage &image)
    : PreviewDialog(parent, image, tr("旋转"))
{
    m_angle = new QSlider;
    m_angleSpin = new QSpinBox;
    m_angleSpin->setRange(-180, 180);
    m_bgButton = new QPushButton(tr("背景颜色…"));
    updateColorButton(m_bgButton, m_bg);

    connect(m_angle, &QSlider::valueChanged, m_angleSpin, &QSpinBox::setValue);
    connect(m_angleSpin, &QSpinBox::valueChanged, m_angle, &QSlider::setValue);
    auto onChanged = [this](int) { refreshPreview(); };
    connect(m_angle, &QSlider::valueChanged, this, onChanged);
    connect(m_bgButton, &QPushButton::clicked, this, &RotateDialog::pickColor);

    m_angle->setValue(15);

    auto *row = new QWidget;
    auto *lay = new QHBoxLayout(row);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(new QLabel(tr("角度")));
    lay->addWidget(m_angle, 1);
    lay->addWidget(m_angleSpin);
    lay->addWidget(m_bgButton);

    auto *outer = new QVBoxLayout;
    outer->addWidget(row);
    addOptions(outer);
}

double RotateDialog::angle() const { return m_angleSpin->value(); }

QImage RotateDialog::generate(const QImage &src) const
{
    if (m_angleSpin->value() == 0)
        return src;
    return ImageOps::rotate(src, m_angleSpin->value(), m_bg);
}

void RotateDialog::resetValues()
{
    m_angle->setValue(0);
    m_bg = Qt::white;
    updateColorButton(m_bgButton, m_bg);
}

void RotateDialog::pickColor()
{
    const QColor c = QColorDialog::getColor(m_bg, this, tr("选择背景颜色"));
    if (c.isValid()) {
        m_bg = c;
        updateColorButton(m_bgButton, m_bg);
        refreshPreview();
    }
}

// ============================================================================
// ResizeDialog
// ============================================================================

ResizeDialog::ResizeDialog(QWidget *parent, const QSize &currentSize)
    : QDialog(parent)
    , m_original(currentSize)
{
    setWindowTitle(tr("调整图像大小"));
    setModal(true);

    m_width = new QSpinBox;
    m_height = new QSpinBox;
    for (QSpinBox *s : { m_width, m_height }) {
        s->setRange(1, 30000);
        s->setMaximumWidth(120);
    }
    m_width->setValue(currentSize.width());
    m_height->setValue(currentSize.height());

    m_keepAspect = new QCheckBox(tr("锁定纵横比"));
    m_keepAspect->setChecked(true);

    connect(m_width, &QSpinBox::valueChanged, this, &ResizeDialog::onWidthChanged);
    connect(m_height, &QSpinBox::valueChanged, this, &ResizeDialog::onHeightChanged);

    auto *form = new QFormLayout;
    form->addRow(tr("宽度（像素）"), m_width);
    form->addRow(tr("高度（像素）"), m_height);
    form->addRow(QString(), m_keepAspect);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(btns);
}

QSize ResizeDialog::targetSize() const
{
    return QSize(m_width->value(), m_height->value());
}

void ResizeDialog::onWidthChanged(int w)
{
    if (m_keepAspect->isChecked() && m_original.width() > 0) {
        QSignalBlocker b(m_height);
        m_height->setValue(qMax(1, int(qRound(
            qreal(w) * m_original.height() / m_original.width()))));
    }
}

void ResizeDialog::onHeightChanged(int h)
{
    if (m_keepAspect->isChecked() && m_original.height() > 0) {
        QSignalBlocker b(m_width);
        m_width->setValue(qMax(1, int(qRound(
            qreal(h) * m_original.width() / m_original.height()))));
    }
}

// ============================================================================
// CanvasResizeDialog
// ============================================================================

CanvasResizeDialog::CanvasResizeDialog(QWidget *parent, const QSize &currentSize)
    : QDialog(parent)
{
    setWindowTitle(tr("调整画布大小"));
    setModal(true);

    m_width = new QSpinBox;
    m_height = new QSpinBox;
    for (QSpinBox *s : { m_width, m_height }) {
        s->setRange(1, 30000);
        s->setMaximumWidth(120);
    }
    m_width->setValue(currentSize.width());
    m_height->setValue(currentSize.height());

    m_bgButton = new QPushButton(tr("背景颜色…"));
    updateColorButton(m_bgButton, m_bg);
    connect(m_bgButton, &QPushButton::clicked, this, &CanvasResizeDialog::pickColor);

    // 九宫格锚点
    auto *anchorBox = new QWidget;
    auto *grid = new QGridLayout(anchorBox);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    for (int i = 0; i < 9; ++i) {
        auto *btn = new QToolButton;
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setFixedSize(28, 28);
        if (i == m_anchor)
            btn->setChecked(true);
        grid->addWidget(btn, i / 3, i % 3);
        connect(btn, &QToolButton::clicked, this, [this, i] { m_anchor = i; });
    }

    auto *form = new QFormLayout;
    form->addRow(tr("画布宽度（像素）"), m_width);
    form->addRow(tr("画布高度（像素）"), m_height);
    form->addRow(tr("原图位置"), anchorBox);
    form->addRow(tr("扩展区域"), m_bgButton);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(btns);
}

QSize CanvasResizeDialog::targetSize() const
{
    return QSize(m_width->value(), m_height->value());
}

void CanvasResizeDialog::pickColor()
{
    const QColor c = QColorDialog::getColor(m_bg, this, tr("选择背景颜色"));
    if (c.isValid()) {
        m_bg = c;
        updateColorButton(m_bgButton, m_bg);
    }
}

// ============================================================================
// TextDialog
// ============================================================================

TextDialog::TextDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("添加文字"));
    setModal(true);

    m_font = QFont(tr("微软雅黑"), 32);
    m_font.setBold(true);

    m_edit = new QPlainTextEdit;
    m_edit->setPlainText(tr("双击修改文字"));
    m_edit->setMinimumWidth(360);

    m_fontButton = new QPushButton(tr("字体…"));
    m_colorButton = new QPushButton(tr("颜色…"));
    connect(m_fontButton, &QPushButton::clicked, this, &TextDialog::pickFont);
    connect(m_colorButton, &QPushButton::clicked, this, &TextDialog::pickColor);

    auto *btnRow = new QWidget;
    auto *btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->addWidget(m_fontButton);
    btnLay->addWidget(m_colorButton);
    btnLay->addStretch(1);

    auto *btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *lay = new QVBoxLayout(this);
    lay->addWidget(m_edit);
    lay->addWidget(btnRow);
    lay->addWidget(btns);
}

QString TextDialog::text() const
{
    return m_edit->toPlainText();
}

void TextDialog::pickFont()
{
    bool ok = false;
    const QFont f = QFontDialog::getFont(&ok, m_font, this, tr("选择字体"));
    if (ok)
        m_font = f;
}

void TextDialog::pickColor()
{
    const QColor c = QColorDialog::getColor(m_color, this, tr("选择文字颜色"));
    if (c.isValid())
        m_color = c;
}
