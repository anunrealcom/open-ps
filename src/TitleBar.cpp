#include "TitleBar.h"
#include "IconFactory.h"
#include "Theme.h"

#include <QLabel>
#include <QToolButton>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWindow>

TitleBar::TitleBar(QWidget *parent)
    : QWidget(parent)
{
    setObjectName("TitleBar");
    setFixedHeight(40);
    setMouseTracking(true);

    m_logo = new QLabel(this);
    m_logo->setPixmap(Theme::logo(20));
    m_logo->setFixedSize(24, 24);
    m_logo->setScaledContents(true);

    m_title = new QLabel("PictureTool", this);
    m_title->setObjectName("AppTitle");

    m_minBtn = makeButton("win-min", "TitleBtn");
    m_maxBtn = makeButton("win-max", "TitleBtn");
    m_closeBtn = makeButton("win-close", "CloseBtn");

    connect(m_minBtn, &QToolButton::clicked, this, &TitleBar::minimizeRequested);
    connect(m_maxBtn, &QToolButton::clicked, this, &TitleBar::maximizeToggleRequested);
    connect(m_closeBtn, &QToolButton::clicked, this, &TitleBar::closeRequested);

    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(12, 0, 8, 0);
    lay->setSpacing(8);
    lay->addWidget(m_logo);
    lay->addWidget(m_title);
    lay->addStretch(1);
    lay->addWidget(m_minBtn);
    lay->addWidget(m_maxBtn);
    lay->addWidget(m_closeBtn);
}

QToolButton *TitleBar::makeButton(const QString &iconName, const char *objName)
{
    auto *btn = new QToolButton(this);
    btn->setObjectName(objName);
    btn->setFixedSize(34, 30);
    btn->setCursor(Qt::ArrowCursor);
    btn->setFocusPolicy(Qt::NoFocus);
    btn->setIcon(IconFactory::icon(iconName, Theme::textPrimary()));
    btn->setIconSize(QSize(18, 18));
    return btn;
}

void TitleBar::setMaximizedState(bool maximized)
{
    m_maximized = maximized;
    m_maxBtn->setIcon(IconFactory::icon(maximized ? "win-restore" : "win-max",
                                        Theme::textPrimary()));
}

void TitleBar::setTitleText(const QString &text)
{
    if (m_title)
        m_title->setText(text);
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (auto *wh = window()->windowHandle()) {
            if (m_maximized) {
                // 最大化状态先还原再拖动，行为更接近系统标题栏
                window()->showNormal();
            }
            wh->startSystemMove();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        emit maximizeToggleRequested();
    QWidget::mouseDoubleClickEvent(event);
}
