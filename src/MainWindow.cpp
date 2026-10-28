#include "MainWindow.h"
#include "ImageCanvas.h"
#include "LayerPanel.h"
#include "ToolBox.h"
#include "ToolOptions.h"
#include "ImageOps.h"
#include "Painters.h"
#include "Commands.h"
#include "dialogs.h"
#include "IconFactory.h"
#include "Theme.h"
#include "TitleBar.h"
#include "AiServer.h"

#include <QAction>
#include <QActionGroup>
#include <QMenuBar>
#include <QMenu>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QSlider>
#include <QSplitter>
#include <QFrame>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QImageReader>
#include <QImageWriter>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QPainter>
#include <QColorDialog>
#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QGuiApplication>
#include <QWindow>
#include <QGraphicsDropShadowEffect>
#include <QEvent>
#include <QShowEvent>
#include <QTimer>
#include <QJsonArray>
#include <QJsonValue>
#include <QByteArray>
#include <QBuffer>
#include <functional>

MainWindow::MainWindow(QWidget *parent)
    : QWidget(parent)
{
    setAcceptDrops(true);
    resize(1280, 820);

    setupFrameless();
    buildUi();

    m_undoStack = new QUndoStack(this);
    m_undoStack->setUndoLimit(60);
    connect(m_undoStack, &QUndoStack::cleanChanged, this, [this](bool) {
        m_modified = !m_undoStack->isClean();
        updateCaption();
    });

    buildMenus();
    buildToolBar();
    buildStatusBar();
    buildConnections();

    // 初始：新建一个默认文档，方便 AI / 画图立即可用
    newDocument(900, 600, Qt::white);
    m_undoStack->setClean();

    // 默认选中画笔，新建空白页即可直接绘画
    m_canvas->setPenColor(Qt::black);
    m_canvas->setPenWidth(4);
    m_canvas->setBrushSize(4);
    if (m_toolOptions) m_toolOptions->setSize(4);
    selectTool(ImageCanvas::Tool::Brush);

    m_ai = new AiServer(this, this);
    if (m_ai->start(48123))
        m_statusBar->showMessage(tr("AI 接口已启动: http://127.0.0.1:48123/command"), 6000);
    else
        m_statusBar->showMessage(tr("AI 接口启动失败（端口占用？）"), 6000);

    applyFrameMode();
    updateCaption();
    refreshActionStates();
}

// ============================================================================
// 无边框窗口结构
// ============================================================================

void MainWindow::setupFrameless()
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
}

void MainWindow::buildUi()
{
    m_outerLayout = new QVBoxLayout(this);
    m_outerLayout->setContentsMargins(m_border, m_border, m_border, m_border);
    m_outerLayout->setSpacing(0);

    m_frame = new QWidget(this);
    m_frame->setObjectName("RootFrame");

    m_shadow = new QGraphicsDropShadowEffect(m_frame);
    m_shadow->setBlurRadius(26);
    m_shadow->setOffset(0, 3);
    m_shadow->setColor(QColor(20, 30, 50, 55));
    m_frame->setGraphicsEffect(m_shadow);

    auto *inner = new QVBoxLayout(m_frame);
    inner->setContentsMargins(0, 0, 0, 0);
    inner->setSpacing(0);

    m_titleBar = new TitleBar(m_frame);
    m_menuBar = new QMenuBar(m_frame);
    m_menuBar->setNativeMenuBar(false);
    m_toolBar = new QToolBar(m_frame);
    m_statusBar = new QStatusBar(m_frame);
    m_statusBar->setSizeGripEnabled(false);

    m_canvas = new ImageCanvas(m_frame);
    m_layerPanel = new LayerPanel(m_frame);
    m_toolBox = new ToolBox(m_frame);
    m_toolOptions = new ToolOptions(m_frame);

    // 右侧：选项面板 + 图层面板
    auto *rightPanel = new QWidget(m_frame);
    auto *rightLay = new QVBoxLayout(rightPanel);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(0);
    rightLay->addWidget(m_toolOptions);
    auto *sep = new QFrame(rightPanel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color:#E6E9EE;");
    rightLay->addWidget(sep);
    rightLay->addWidget(m_layerPanel, 1);

    auto *split = new QSplitter(Qt::Horizontal, m_frame);
    split->setHandleWidth(1);
    split->setChildrenCollapsible(false);
    split->addWidget(m_toolBox);
    split->addWidget(m_canvas);
    split->addWidget(rightPanel);
    split->setStretchFactor(0, 0);
    split->setStretchFactor(1, 1);
    split->setStretchFactor(2, 0);
    split->setSizes({ 118, 980, 240 });

    inner->addWidget(m_titleBar);
    inner->addWidget(m_menuBar);
    inner->addWidget(m_toolBar);
    inner->addWidget(split, 1);
    inner->addWidget(m_statusBar);

    m_outerLayout->addWidget(m_frame);

    connect(m_titleBar, &TitleBar::minimizeRequested, this, &QWidget::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeToggleRequested, this, &MainWindow::toggleMaximize);
    connect(m_titleBar, &TitleBar::closeRequested, this, &QWidget::close);
}

void MainWindow::applyFrameMode()
{
    const int r = m_maximized ? 0 : 14;
    const int margin = m_maximized ? 0 : m_border;
    m_outerLayout->setContentsMargins(margin, margin, margin, margin);

    m_frame->setStyleSheet(QString(
        "#RootFrame { background: %1; border: 1px solid %2; border-radius: %3px; }")
        .arg(Theme::windowBg().name(), Theme::borderClr().name()).arg(r));

    m_titleBar->setStyleSheet(QString(
        "#TitleBar { background: transparent; border-top-left-radius: %1px; "
        "border-top-right-radius: %1px; }").arg(r));

    m_frame->setGraphicsEffect(m_maximized ? nullptr : m_shadow);
}

void MainWindow::toggleMaximize()
{
    if (!m_maximized) {
        m_normalGeom = geometry();
        QScreen *scr = QGuiApplication::screenAt(geometry().center());
        if (!scr) scr = QGuiApplication::primaryScreen();
        m_maximized = true;
        applyFrameMode();
        if (scr) setGeometry(scr->availableGeometry());
    } else {
        m_maximized = false;
        applyFrameMode();
        setGeometry(m_normalGeom);
    }
    m_titleBar->setMaximizedState(m_maximized);
}

Qt::Edges MainWindow::resizeEdgesAt(const QPoint &pos) const
{
    Qt::Edges e;
    if (m_maximized)
        return e;
    if (pos.x() <= m_border)               e |= Qt::LeftEdge;
    if (pos.y() <= m_border)               e |= Qt::TopEdge;
    if (pos.x() >= width() - m_border)     e |= Qt::RightEdge;
    if (pos.y() >= height() - m_border)    e |= Qt::BottomEdge;
    return e;
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        const Qt::Edges edges = resizeEdgesAt(event->pos());
        if (edges && windowHandle()) {
            windowHandle()->startSystemResize(edges);
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    const Qt::Edges e = resizeEdgesAt(event->pos());
    Qt::CursorShape cs = Qt::ArrowCursor;
    const bool l = e & Qt::LeftEdge, r = e & Qt::RightEdge;
    const bool t = e & Qt::TopEdge, b = e & Qt::BottomEdge;
    if ((l && t) || (r && b)) cs = Qt::SizeFDiagCursor;
    else if ((r && t) || (l && b)) cs = Qt::SizeBDiagCursor;
    else if (l || r) cs = Qt::SizeHorCursor;
    else if (t || b) cs = Qt::SizeVerCursor;
    setCursor(cs);
    QWidget::mouseMoveEvent(event);
}

void MainWindow::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::ActivationChange)
        update();
}

// ============================================================================
// 菜单 / 工具栏 / 状态栏
// ============================================================================

void MainWindow::buildMenus()
{
    const QColor ic = Theme::accent();

    QMenu *fileMenu = m_menuBar->addMenu(IconFactory::icon("file", ic), tr("文件(&F)"));
    fileMenu->addAction(IconFactory::icon("new", ic), tr("新建(&N)…"), QKeySequence::New,
                        this, &MainWindow::newFile);
    fileMenu->addAction(IconFactory::icon("open", ic), tr("打开(&O)…"), QKeySequence::Open,
                        this, &MainWindow::openFile);
    fileMenu->addAction(IconFactory::icon("image", ic), tr("导入图片为图层(&M)…"),
                        QKeySequence("Ctrl+Shift+O"), this, &MainWindow::importImageLayer);
    fileMenu->addAction(IconFactory::icon("save", ic), tr("保存(&S)"), QKeySequence::Save,
                        this, &MainWindow::saveFile);
    fileMenu->addAction(IconFactory::icon("save", ic), tr("另存为(&A)…"), QKeySequence::SaveAs,
                        this, &MainWindow::saveFileAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("退出(&X)"), QKeySequence::Quit, this, &QWidget::close);

    QMenu *editMenu = m_menuBar->addMenu(IconFactory::icon("edit", ic), tr("编辑(&E)"));
    auto *undoAct = m_undoStack->createUndoAction(this, tr("撤销(&U)"));
    undoAct->setShortcut(QKeySequence::Undo);
    undoAct->setIcon(IconFactory::icon("undo", ic));
    auto *redoAct = m_undoStack->createRedoAction(this, tr("重做(&R)"));
    redoAct->setShortcut(QKeySequence::Redo);
    redoAct->setIcon(IconFactory::icon("redo", ic));
    editMenu->addAction(undoAct);
    editMenu->addAction(redoAct);

    // 工具
    QMenu *toolMenu = m_menuBar->addMenu(IconFactory::icon("draw", ic), tr("工具(&T)"));
    struct TItem { const char *label; const char *iconName; ImageCanvas::Tool tool; const char *key; };
    const TItem items[] = {
        {"移动视图", "pan",     ImageCanvas::Tool::Pan,     "V"},
        {"铅笔",     "pencil",  ImageCanvas::Tool::Pencil,  "N"},
        {"画笔",     "brush",   ImageCanvas::Tool::Brush,   "B"},
        {"橡皮",     "eraser",  ImageCanvas::Tool::Eraser,  "K"},
        {"直线",     "line",    ImageCanvas::Tool::Line,    "L"},
        {"矩形",     "rect",    ImageCanvas::Tool::Rect,    "U"},
        {"椭圆",     "ellipse", ImageCanvas::Tool::Ellipse, "M"},
        {"油漆桶",   "fill",    ImageCanvas::Tool::Fill,    "F"},
        {"取色器",   "picker",  ImageCanvas::Tool::Picker,  "I"},
        {"文字",     "text",    ImageCanvas::Tool::TextTool,"T"},
        {"裁剪",     "crop",    ImageCanvas::Tool::Crop,    "C"},
    };
    m_toolGroup = new QActionGroup(this);
    for (const TItem &it : items) {
        QAction *a = toolMenu->addAction(
            IconFactory::icon(QString::fromUtf8(it.iconName), ic),
            tr(it.label), QKeySequence(it.key), this,
            [this, tool = it.tool] { selectTool(tool); });
        a->setCheckable(true);
        m_toolGroup->addAction(a);
    }
    toolMenu->addSeparator();
    toolMenu->addAction(IconFactory::icon("color", ic), tr("前景色…"),
                        QKeySequence("Ctrl+Shift+P"), this, &MainWindow::pickPenColor);

    // 图像
    QMenu *imageMenu = m_menuBar->addMenu(IconFactory::icon("image", ic), tr("图像(&I)"));
    imageMenu->addAction(IconFactory::icon("adjust", ic), tr("亮度/对比度/饱和度/色温…"),
                         QKeySequence("Ctrl+M"), this, &MainWindow::showAdjustDialog);
    QMenu *filterMenu = imageMenu->addMenu(IconFactory::icon("filter", ic), tr("滤镜"));
    filterMenu->addAction(IconFactory::icon("grayscale", ic), tr("灰度"), QKeySequence("Ctrl+G"),
                          this, &MainWindow::applyGrayscale);
    filterMenu->addAction(IconFactory::icon("invert", ic), tr("反色"), QKeySequence("Ctrl+I"),
                          this, &MainWindow::applyInvert);
    filterMenu->addAction(IconFactory::icon("levels", ic), tr("自动色阶"), QKeySequence("Ctrl+L"),
                          this, &MainWindow::applyAutoLevels);
    filterMenu->addSeparator();
    filterMenu->addAction(IconFactory::icon("blur", ic), tr("高斯模糊…"), QKeySequence("Ctrl+B"),
                          this, &MainWindow::showBlurDialog);
    filterMenu->addAction(IconFactory::icon("sharpen", ic), tr("锐化…"), QKeySequence("Ctrl+Alt+S"),
                          this, &MainWindow::showSharpenDialog);

    QMenu *rotateMenu = imageMenu->addMenu(IconFactory::icon("rotate", ic), tr("旋转/翻转"));
    rotateMenu->addAction(IconFactory::icon("rotate", ic), tr("任意角度旋转…"), QKeySequence("Ctrl+R"),
                          this, &MainWindow::showRotateDialog);
    rotateMenu->addAction(IconFactory::icon("rotate", ic), tr("顺时针 90°"), QKeySequence("Ctrl+Shift+R"),
                          this, &MainWindow::rotate90CW);
    rotateMenu->addAction(IconFactory::icon("rotate", ic), tr("逆时针 90°"), QKeySequence("Ctrl+Alt+R"),
                          this, &MainWindow::rotate90CCW);
    rotateMenu->addAction(IconFactory::icon("rotate", ic), tr("180°"), QKeySequence("Ctrl+Alt+Shift+R"),
                          this, &MainWindow::rotate180);
    rotateMenu->addSeparator();
    rotateMenu->addAction(IconFactory::icon("flip", ic), tr("水平翻转"), QKeySequence("Ctrl+Shift+H"),
                          this, &MainWindow::flipHorizontal);
    rotateMenu->addAction(IconFactory::icon("flip", ic), tr("垂直翻转"), QKeySequence("Ctrl+Shift+V"),
                          this, &MainWindow::flipVertical);
    imageMenu->addSeparator();
    imageMenu->addAction(IconFactory::icon("crop", ic), tr("应用裁剪"), QKeySequence("Ctrl+Shift+C"),
                         this, &MainWindow::applyCrop);
    imageMenu->addAction(IconFactory::icon("resize", ic), tr("调整图像大小…"), QKeySequence("Ctrl+Shift+Z"),
                         this, &MainWindow::showResizeDialog);
    imageMenu->addAction(IconFactory::icon("canvas", ic), tr("调整画布大小…"), QKeySequence("Ctrl+Shift+Alt+C"),
                         this, &MainWindow::showCanvasResizeDialog);

    // 图层
    QMenu *layerMenu = m_menuBar->addMenu(IconFactory::icon("layers", ic), tr("图层(&L)"));
    layerMenu->addAction(IconFactory::icon("new", ic), tr("新建图层"), QKeySequence("Ctrl+Shift+N"),
                         this, &MainWindow::onLayerAdd);
    layerMenu->addAction(IconFactory::icon("copy", ic), tr("复制图层"), QKeySequence("Ctrl+D"),
                         this, [this] { onLayerDuplicate(m_doc.activeIndex()); });
    layerMenu->addAction(IconFactory::icon("delete", ic), tr("删除图层"), QKeySequence(),
                         this, [this] { onLayerRemove(m_doc.activeIndex()); });
    layerMenu->addSeparator();
    layerMenu->addAction(tr("上移图层"), QKeySequence("Ctrl+Up"),
                         this, [this] { onLayerMove(m_doc.activeIndex(), m_doc.activeIndex() + 1); });
    layerMenu->addAction(tr("下移图层"), QKeySequence("Ctrl+Down"),
                         this, [this] { onLayerMove(m_doc.activeIndex(), m_doc.activeIndex() - 1); });
    layerMenu->addSeparator();
    layerMenu->addAction(IconFactory::icon("clear", ic), tr("清空当前图层"),
                         QKeySequence("Ctrl+Backspace"), this, &MainWindow::clearActiveLayer);
    layerMenu->addAction(IconFactory::icon("clear", ic), tr("清空所有图层"),
                         QKeySequence("Ctrl+Shift+Backspace"), this, &MainWindow::clearAllLayers);

    // 水印消除
    QMenu *wmMenu = m_menuBar->addMenu(IconFactory::icon("watermark", ic), tr("水印消除(&W)"));
    wmMenu->addAction(IconFactory::icon("mask", ic), tr("遮罩画笔"), QKeySequence("Shift+W"),
                      this, [this] { selectTool(ImageCanvas::Tool::MaskBrush); });
    wmMenu->addAction(IconFactory::icon("mask", ic), tr("遮罩橡皮"), QKeySequence("Shift+E"),
                      this, [this] { selectTool(ImageCanvas::Tool::MaskEraser); });
    wmMenu->addAction(IconFactory::icon("mask", ic), tr("矩形遮罩"), QKeySequence("Shift+R"),
                      this, [this] { selectTool(ImageCanvas::Tool::MaskRect); });
    wmMenu->addAction(IconFactory::icon("mask", ic), tr("清除遮罩"), QKeySequence("Ctrl+Shift+W"),
                      this, [this] { m_canvas->clearMask(); });
    wmMenu->addSeparator();
    wmMenu->addAction(IconFactory::icon("watermark", ic), tr("消除水印（应用修补）"), QKeySequence("Ctrl+W"),
                      this, &MainWindow::applyInpaint);

    // 视图
    QMenu *viewMenu = m_menuBar->addMenu(IconFactory::icon("view", ic), tr("视图(&V)"));
    viewMenu->addAction(IconFactory::icon("zoomin", ic), tr("放大"), QKeySequence::ZoomIn,
                        this, [this] { m_canvas->zoomIn(); });
    viewMenu->addAction(IconFactory::icon("zoomout", ic), tr("缩小"), QKeySequence::ZoomOut,
                        this, [this] { m_canvas->zoomOut(); });
    viewMenu->addAction(IconFactory::icon("fit", ic), tr("适合窗口"), QKeySequence("Ctrl+0"),
                        this, [this] { m_canvas->zoomToFit(); });
    viewMenu->addAction(IconFactory::icon("actual", ic), tr("实际大小 (100%)"), QKeySequence("Ctrl+1"),
                        this, [this] { m_canvas->zoomToActual(); });
    viewMenu->addSeparator();
    auto *compareAct = viewMenu->addAction(IconFactory::icon("compare", ic), tr("对比原图"));
    compareAct->setCheckable(true);
    connect(compareAct, &QAction::toggled, this, &MainWindow::toggleCompare);

    // 帮助
    QMenu *helpMenu = m_menuBar->addMenu(IconFactory::icon("help", ic), tr("帮助(&H)"));
    helpMenu->addAction(IconFactory::icon("info", ic), tr("关于 PictureTool"),
                        QKeySequence::HelpContents, this, &MainWindow::showAbout);

    std::function<void(QMenu *)> polishMenu = [&](QMenu *m) {
        m->setWindowFlags(m->windowFlags() | Qt::FramelessWindowHint);
        m->setAttribute(Qt::WA_TranslucentBackground);
        for (QAction *a : m->actions())
            if (QMenu *sub = a->menu()) polishMenu(sub);
    };
    for (QAction *a : m_menuBar->actions())
        if (QMenu *m = a->menu()) polishMenu(m);
}

void MainWindow::buildToolBar()
{
    m_toolBar->setMovable(false);
    m_toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_toolBar->setIconSize(QSize(18, 18));

    auto act = [this](const QString &iconName, const QString &text, auto &&fn) -> QAction * {
        QAction *a = new QAction(IconFactory::icon(iconName, Theme::accent()), text, this);
        connect(a, &QAction::triggered, this, std::forward<decltype(fn)>(fn));
        return a;
    };

    m_toolBar->addAction(act("new", tr("新建"), [this] { newFile(); }));
    m_toolBar->addAction(act("open", tr("打开"), [this] { openFile(); }));
    m_toolBar->addAction(act("save", tr("保存"), [this] { saveFile(); }));
    m_toolBar->addAction(act("image", tr("导入图片"), [this] { importImageLayer(); }));
    m_toolBar->addSeparator();
    m_toolBar->addAction(act("undo", tr("撤销"), [this] { m_undoStack->undo(); }));
    m_toolBar->addAction(act("redo", tr("重做"), [this] { m_undoStack->redo(); }));
    m_toolBar->addSeparator();
    m_toolBar->addAction(act("adjust", tr("调色"), [this] { showAdjustDialog(); }));
    m_toolBar->addAction(act("watermark", tr("框选去水印"), [this] { startAutoInpaint(); }));
    m_toolBar->addAction(act("clear", tr("清空"), [this] { clearActiveLayer(); }));
    m_toolBar->addSeparator();
    m_toolBar->addAction(act("fit", tr("适合窗口"), [this] { m_canvas->zoomToFit(); }));
    m_toolBar->addAction(act("actual", tr("100%"), [this] { m_canvas->zoomToActual(); }));
}

void MainWindow::buildStatusBar()
{
    m_sizeLabel = new QLabel(tr("无图像"), m_statusBar);
    m_posLabel = new QLabel(m_statusBar);
    m_zoomLabel = new QLabel(m_statusBar);
    m_sizeLabel->setMinimumWidth(140);
    m_posLabel->setMinimumWidth(150);
    m_zoomLabel->setMinimumWidth(70);
    m_statusBar->addWidget(m_sizeLabel);
    m_statusBar->addWidget(m_posLabel);
    m_statusBar->addPermanentWidget(m_zoomLabel);
}

void MainWindow::buildConnections()
{
    connect(m_canvas, &ImageCanvas::commitLayerEdit,
            this, &MainWindow::onCommitLayerEdit);
    connect(m_canvas, &ImageCanvas::applyCropRequested, this, &MainWindow::applyCrop);
    connect(m_canvas, &ImageCanvas::textRequested, this, &MainWindow::addTextAt);
    connect(m_canvas, &ImageCanvas::colorPicked, this, &MainWindow::onColorPicked);
    connect(m_canvas, &ImageCanvas::fileDropped, this, &MainWindow::onFileDropped);
    connect(m_canvas, &ImageCanvas::zoomChanged, this, &MainWindow::onZoomChanged);
    connect(m_canvas, &ImageCanvas::cursorMoved, this, &MainWindow::onCursorMoved);
    connect(m_canvas, &ImageCanvas::cropRectChanged, this, &MainWindow::onCropRectChanged);
    connect(m_canvas, &ImageCanvas::maskChanged, this, &MainWindow::onMaskChanged);
    connect(m_canvas, &ImageCanvas::maskRectFinished, this, &MainWindow::onMaskRectFinished);

    connect(m_layerPanel, &LayerPanel::requestAddLayer, this, &MainWindow::onLayerAdd);
    connect(m_layerPanel, &LayerPanel::requestRemoveLayer, this, &MainWindow::onLayerRemove);
    connect(m_layerPanel, &LayerPanel::requestDuplicateLayer, this, &MainWindow::onLayerDuplicate);
    connect(m_layerPanel, &LayerPanel::requestMoveLayer, this, &MainWindow::onLayerMove);
    connect(m_layerPanel, &LayerPanel::requestSetActive, this, &MainWindow::onLayerSetActive);
    connect(m_layerPanel, &LayerPanel::requestSetVisible, this, &MainWindow::onLayerSetVisible);
    connect(m_layerPanel, &LayerPanel::requestSetOpacity, this, &MainWindow::onLayerSetOpacity);
    connect(m_layerPanel, &LayerPanel::requestSetBlend, this, &MainWindow::onLayerSetBlend);
    connect(m_layerPanel, &LayerPanel::requestRename, this, &MainWindow::onLayerRename);

    connect(m_toolBox, &ToolBox::toolChosen, this, [this](ImageCanvas::Tool t) { selectTool(t); });
    connect(m_toolOptions, &ToolOptions::colorChosen, this, [this](const QColor &c) {
        m_canvas->setPenColor(c);
    });
    connect(m_toolOptions, &ToolOptions::sizeChanged, this, [this](int s) {
        m_canvas->setPenWidth(s);
        m_canvas->setBrushSize(s);
        if (m_penWidthSlider) {
            QSignalBlocker b(m_penWidthSlider);
            m_penWidthSlider->setValue(qMin(80, s));
        }
    });
    connect(m_toolOptions, &ToolOptions::importRequested, this, &MainWindow::importImageLayer);
}

void MainWindow::selectTool(ImageCanvas::Tool tool)
{
    m_canvas->setTool(tool);
    if (m_toolBox)
        m_toolBox->setTool(tool);
    m_statusBar->showMessage(tr("当前工具: %1").arg(m_canvas->toolName()), 2000);
}

// ============================================================================
// 撤销 / 文档编辑核心
// ============================================================================

void MainWindow::applyDocument(const Document &d)
{
    m_doc = d;
    m_canvas->invalidate();
    m_layerPanel->refresh(m_doc);
    refreshActionStates();
}

void MainWindow::commitEdit(const Document &before, const Document &after, const QString &desc)
{
    auto *cmd = new DocCommand(before, after, desc,
                               [this](const Document &d) { applyDocument(d); });
    m_undoStack->push(cmd);   // push 会调用 redo() -> applyDocument(after)
}

void MainWindow::onCommitLayerEdit(int layerIndex, const QImage &beforeImage, const QString &desc)
{
    if (layerIndex < 0 || layerIndex >= m_doc.layerCount())
        return;
    Document after = m_doc;              // 画布已把新像素写入 m_doc
    Document before = m_doc;
    before.layerAt(layerIndex).image = beforeImage;
    commitEdit(before, after, desc);
}

// 对活动图层应用一张新图（滤镜/修补/文字等）
void MainWindow::applyLayerOp(const QString &desc, const QImage &newActiveLayer)
{
    if (!m_doc.activeLayer())
        return;
    Document before = m_doc;
    m_doc.activeLayer()->image = newActiveLayer.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    commitEdit(before, m_doc, desc);
}

// ============================================================================
// 文件
// ============================================================================

void MainWindow::newFile()
{
    if (!maybeSave())
        return;
    bool ok = false;
    const QSize vp = m_canvas->size();
    int w = QInputDialog::getInt(this, tr("新建"), tr("宽度(像素):"),
                                 qMax(200, vp.width() - 8), 1, 12000, 1, &ok);
    if (!ok) return;
    int h = QInputDialog::getInt(this, tr("新建"), tr("高度(像素):"),
                                 qMax(200, vp.height() - 8), 1, 12000, 1, &ok);
    if (!ok) return;
    newDocument(w, h, Qt::white);
    setCurrentFile(QString());
    m_undoStack->clear();
    m_undoStack->setClean();
}

void MainWindow::newDocument(int w, int h, const QColor &bg)
{
    m_doc = Document(w, h, tr("背景"));
    if (bg.alpha() > 0 && bg != Qt::transparent)
        m_doc.layerAt(0).image.fill(bg);
    m_canvas->setDocument(&m_doc);
    m_canvas->zoomToFit();
    m_layerPanel->refresh(m_doc);
    refreshActionStates();
}

void MainWindow::fitBlankDocToView()
{
    if (!m_filePath.isEmpty())
        return;   // 已加载文件则保持原尺寸
    const QSize vp = m_canvas->size();
    const int w = qMax(200, vp.width() - 8);
    const int h = qMax(200, vp.height() - 8);
    m_doc = Document(w, h, tr("背景"));
    m_doc.layerAt(0).image.fill(Qt::white);
    m_canvas->setDocument(&m_doc);
    m_canvas->zoomToActual();   // 100%，铺满整个绘图区
    m_layerPanel->refresh(m_doc);
    m_undoStack->clear();
    m_undoStack->setClean();
    refreshActionStates();
}

void MainWindow::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_initialized) {
        m_initialized = true;
        // 布局完成后画布尺寸才有效，延迟到事件循环再铺满
        QTimer::singleShot(0, this, [this] { fitBlankDocToView(); });
    }
}

bool MainWindow::openPath(const QString &path)
{
    if (!maybeSave())
        return false;
    return loadFile(path);
}

void MainWindow::openFile()
{
    if (!maybeSave())
        return;
    const QString path = QFileDialog::getOpenFileName(
        this, tr("打开图片"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp *.ppm *.xpm);;所有文件 (*.*)"));
    if (!path.isEmpty())
        loadFile(path);
}

bool MainWindow::loadFile(const QString &path)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, tr("打开失败"),
                             tr("无法读取图片 %1：\n%2")
                                 .arg(QDir::toNativeSeparators(path), reader.errorString()));
        return false;
    }
    m_doc = Document::fromImage(img, tr("背景"));
    m_canvas->setDocument(&m_doc);
    m_canvas->zoomToFit();
    m_layerPanel->refresh(m_doc);
    m_undoStack->clear();
    m_undoStack->setClean();
    setCurrentFile(path);
    m_statusBar->showMessage(tr("已打开 %1").arg(QDir::toNativeSeparators(path)), 4000);
    refreshActionStates();
    return true;
}

bool MainWindow::saveFile()
{
    if (m_filePath.isEmpty())
        return saveFileAs();
    return writeFile(m_filePath);
}

bool MainWindow::saveFileAs()
{
    QString selectedFilter;
    const QString suggested = m_filePath.isEmpty() ? tr("未命名.png")
                                                   : QFileInfo(m_filePath).fileName();
    QString path = QFileDialog::getSaveFileName(
        this, tr("保存图片"), suggested,
        tr("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg);;BMP 图片 (*.bmp);;"
           "TIFF 图片 (*.tif *.tiff);;WebP 图片 (*.webp)"),
        &selectedFilter);
    if (path.isEmpty())
        return false;
    QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix.isEmpty()) {
        if (selectedFilter.contains("jpg", Qt::CaseInsensitive)) suffix = "jpg";
        else if (selectedFilter.contains("bmp", Qt::CaseInsensitive)) suffix = "bmp";
        else if (selectedFilter.contains("tif", Qt::CaseInsensitive)) suffix = "tif";
        else if (selectedFilter.contains("webp", Qt::CaseInsensitive)) suffix = "webp";
        else suffix = "png";
        path += "." + suffix;
    }
    if (!writeFile(path))
        return false;
    setCurrentFile(path);
    return true;
}

bool MainWindow::writeFile(const QString &path)
{
    if (m_doc.isNull())
        return false;
    QImage img = m_doc.composite();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg") {
        QImage flat(img.size(), QImage::Format_RGB32);
        flat.fill(Qt::white);
        QPainter p(&flat);
        p.drawImage(0, 0, img);
        p.end();
        img = flat;
    }
    QImageWriter writer(path);
    writer.setFormat(suffix.toLatin1());
    writer.setQuality(92);
    if (suffix == "png")
        writer.setCompression(9);
    if (!writer.write(img)) {
        QMessageBox::warning(this, tr("保存失败"),
                             tr("无法写入 %1：\n%2")
                                 .arg(QDir::toNativeSeparators(path), writer.errorString()));
        return false;
    }
    m_undoStack->setClean();
    m_statusBar->showMessage(tr("已保存到 %1").arg(QDir::toNativeSeparators(path)), 4000);
    return true;
}

bool MainWindow::maybeSave()
{
    if (!m_modified)
        return true;
    const auto ret = QMessageBox::warning(
        this, tr("未保存的修改"), tr("当前图片已修改，是否保存？"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (ret == QMessageBox::Save)
        return saveFile();
    return ret != QMessageBox::Cancel;
}

void MainWindow::setCurrentFile(const QString &path) { m_filePath = path; updateCaption(); }

void MainWindow::updateCaption()
{
    const QString name = m_filePath.isEmpty() ? tr("未命名") : QFileInfo(m_filePath).fileName();
    const QString shown = name + (m_modified ? " *" : "");
    setWindowTitle(shown + " - PictureTool");
    if (m_titleBar)
        m_titleBar->setTitleText(shown);
}

void MainWindow::refreshActionStates()
{
    m_sizeLabel->setText(m_doc.isNull() ? tr("无图像")
                                        : tr("%1 × %2 px · %3 图层")
                                              .arg(m_doc.width()).arg(m_doc.height())
                                              .arg(m_doc.layerCount()));
}

// ============================================================================
// 色彩 / 滤镜（活动图层）
// ============================================================================

void MainWindow::showAdjustDialog()
{
    if (m_doc.isNull() || !m_doc.activeLayer()) return;
    AdjustDialog dlg(this, m_doc.activeLayer()->image);
    if (dlg.exec() == QDialog::Accepted)
        applyLayerOp(tr("调整亮度/对比度"), dlg.applyTo(m_doc.activeLayer()->image));
}

void MainWindow::applyGrayscale()
{
    if (!m_doc.activeLayer()) return;
    applyLayerOp(tr("灰度"), ImageOps::toGrayscale(m_doc.activeLayer()->image));
}

void MainWindow::applyInvert()
{
    if (!m_doc.activeLayer()) return;
    applyLayerOp(tr("反色"), ImageOps::invertColors(m_doc.activeLayer()->image));
}

void MainWindow::applyAutoLevels()
{
    if (!m_doc.activeLayer()) return;
    applyLayerOp(tr("自动色阶"), ImageOps::autoLevels(m_doc.activeLayer()->image));
}

void MainWindow::showBlurDialog()
{
    if (!m_doc.activeLayer()) return;
    BlurDialog dlg(this, m_doc.activeLayer()->image);
    if (dlg.exec() == QDialog::Accepted)
        applyLayerOp(tr("高斯模糊"), dlg.applyTo(m_doc.activeLayer()->image));
}

void MainWindow::showSharpenDialog()
{
    if (!m_doc.activeLayer()) return;
    SharpenDialog dlg(this, m_doc.activeLayer()->image);
    if (dlg.exec() == QDialog::Accepted)
        applyLayerOp(tr("锐化"), dlg.applyTo(m_doc.activeLayer()->image));
}

// ============================================================================
// 几何（整个文档）
// ============================================================================

void MainWindow::showRotateDialog()
{
    if (m_doc.isNull()) return;
    RotateDialog dlg(this, m_doc.composite());
    if (dlg.exec() == QDialog::Accepted && qAbs(dlg.angle()) > 0.0001) {
        Document before = m_doc;
        m_doc.rotateAll(dlg.angle(), dlg.background());
        commitEdit(before, m_doc, tr("旋转 %1°").arg(dlg.angle()));
    }
}

void MainWindow::rotate90CW()
{
    if (m_doc.isNull()) return;
    Document before = m_doc; m_doc.rotateAll90(true); commitEdit(before, m_doc, tr("旋转 90°"));
}
void MainWindow::rotate90CCW()
{
    if (m_doc.isNull()) return;
    Document before = m_doc; m_doc.rotateAll90(false); commitEdit(before, m_doc, tr("旋转 -90°"));
}
void MainWindow::rotate180()
{
    if (m_doc.isNull()) return;
    Document before = m_doc; m_doc.rotateAll(180.0, Qt::transparent); commitEdit(before, m_doc, tr("旋转 180°"));
}
void MainWindow::flipHorizontal()
{
    if (m_doc.isNull()) return;
    Document before = m_doc; m_doc.flipAll(true, false); commitEdit(before, m_doc, tr("水平翻转"));
}
void MainWindow::flipVertical()
{
    if (m_doc.isNull()) return;
    Document before = m_doc; m_doc.flipAll(false, true); commitEdit(before, m_doc, tr("垂直翻转"));
}

void MainWindow::applyCrop()
{
    if (m_doc.isNull() || !m_canvas->hasCropRect())
        return;
    const QRect r = m_canvas->cropRect();
    if (r.width() < 2 || r.height() < 2)
        return;
    Document before = m_doc;
    m_doc.cropAll(r);
    commitEdit(before, m_doc, tr("裁剪"));
    m_canvas->clearCropRect();
    m_canvas->setTool(ImageCanvas::Tool::Pan);
    m_canvas->zoomToFit();
}

void MainWindow::showResizeDialog()
{
    if (m_doc.isNull()) return;
    ResizeDialog dlg(this, m_doc.size());
    if (dlg.exec() == QDialog::Accepted) {
        Document before = m_doc;
        m_doc.resizeAll(dlg.targetSize());
        commitEdit(before, m_doc, tr("调整大小"));
        m_canvas->zoomToFit();
    }
}

void MainWindow::showCanvasResizeDialog()
{
    if (m_doc.isNull()) return;
    CanvasResizeDialog dlg(this, m_doc.size());
    if (dlg.exec() == QDialog::Accepted) {
        Document before = m_doc;
        m_doc.resizeCanvasAll(dlg.targetSize(), dlg.anchor(), dlg.background());
        commitEdit(before, m_doc, tr("调整画布"));
        m_canvas->zoomToFit();
    }
}

// ============================================================================
// 水印消除（活动图层）
// ============================================================================

void MainWindow::applyInpaint()
{
    if (!m_doc.activeLayer()) return;
    if (m_canvas->maskIsEmpty()) {
        QMessageBox::information(this, tr("去水印"),
            tr("请先框选要消除的水印区域，再执行去水印。\n\n"
               "最简方式：点工具栏「框选去水印」，然后直接在图片上"
               "拖一个方框框住水印，松手即自动消除。"));
        return;
    }
    setCursor(Qt::WaitCursor);
    const QImage result = ImageOps::inpaint(m_doc.activeLayer()->image, m_canvas->mask());
    unsetCursor();
    applyLayerOp(tr("消除水印"), result);
    m_canvas->clearMask();
    m_statusBar->showMessage(tr("水印消除完成"), 4000);
}

void MainWindow::startAutoInpaint()
{
    if (!m_doc.activeLayer())
        return;
    m_pendingAutoInpaint = true;
    m_canvas->clearMask();
    selectTool(ImageCanvas::Tool::MaskRect);
    m_statusBar->showMessage(tr("请在图片上拖一个方框框住水印，松手即自动消除"), 6000);
}

void MainWindow::onMaskRectFinished()
{
    if (!m_pendingAutoInpaint)
        return;
    m_pendingAutoInpaint = false;
    applyInpaint();
    selectTool(ImageCanvas::Tool::Brush);   // 完成后回到画笔
}

// ============================================================================
// 画笔属性 / 文字 / 取色
// ============================================================================

void MainWindow::pickPenColor()
{
    const QColor c = QColorDialog::getColor(m_canvas->penColor(), this, tr("选择前景色"));
    if (c.isValid())
        m_canvas->setPenColor(c);
}

void MainWindow::setPenWidthFromSlider(int width)
{
    m_canvas->setPenWidth(width);
    m_canvas->setBrushSize(qMax(1, width));
}

void MainWindow::onColorPicked(const QColor &color)
{
    m_canvas->setPenColor(color);
    if (m_toolOptions)
        m_toolOptions->setColor(color);
    m_statusBar->showMessage(tr("已取色: %1").arg(color.name(QColor::HexArgb)), 2500);
}

void MainWindow::clearActiveLayer()
{
    if (!m_doc.activeLayer())
        return;
    if (QMessageBox::question(this, tr("清空图层"),
            tr("确定清空当前图层「%1」？").arg(m_doc.activeLayerName()))
        != QMessageBox::Yes)
        return;
    const int idx = m_doc.activeIndex();
    Document before = m_doc;
    m_doc.clearLayer(idx);
    commitEdit(before, m_doc, tr("清空图层"));
    m_statusBar->showMessage(tr("已清空当前图层"), 3000);
}

void MainWindow::clearAllLayers()
{
    if (m_doc.isNull())
        return;
    if (QMessageBox::question(this, tr("清空画布"),
            tr("确定清空所有图层内容？此操作可撤销。"))
        != QMessageBox::Yes)
        return;
    Document before = m_doc;
    for (int i = 0; i < m_doc.layerCount(); ++i)
        m_doc.clearLayer(i);
    commitEdit(before, m_doc, tr("清空画布"));
    m_statusBar->showMessage(tr("已清空画布"), 3000);
}

void MainWindow::importImageLayer()
{
    if (m_doc.isNull()) {
        m_statusBar->showMessage(tr("请先新建或打开一个文档"), 3000);
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("导入图片为图层"), QString(),
        tr("图片文件 (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp);;所有文件 (*.*)"));
    if (path.isEmpty())
        return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QImage img = reader.read();
    if (img.isNull()) {
        QMessageBox::warning(this, tr("导入失败"),
                             tr("无法读取图片 %1").arg(QDir::toNativeSeparators(path)));
        return;
    }
    Document before = m_doc;
    m_doc.addImageLayer(img, QFileInfo(path).completeBaseName());
    commitEdit(before, m_doc, tr("导入图片"));
    m_statusBar->showMessage(tr("已导入为图层: %1").arg(QFileInfo(path).fileName()), 4000);
}

void MainWindow::addTextAt(const QPointF &imagePos)
{
    if (!m_doc.activeLayer()) return;
    TextDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted || dlg.text().isEmpty())
        return;
    QImage img = m_doc.activeLayer()->image.copy();
    QPainter p(&img);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    p.setFont(dlg.font());
    p.setPen(dlg.color());
    const QStringList lines = dlg.text().split('\n');
    const qreal lineHeight = QFontMetricsF(dlg.font()).height() * 1.35;
    qreal y = imagePos.y();
    for (const QString &line : lines) {
        p.drawText(QPointF(imagePos.x(), y + QFontMetricsF(dlg.font()).ascent()), line);
        y += lineHeight;
    }
    p.end();
    applyLayerOp(tr("添加文字"), img);
}

// ============================================================================
// 视图 / 帮助
// ============================================================================

void MainWindow::toggleCompare(bool on) { m_canvas->setCompareMode(on); }

void MainWindow::showAbout()
{
    QMessageBox::about(this, tr("关于 PictureTool"),
        tr("<h3>PictureTool 1.0</h3>"
           "<p>基于 Qt 6 的多图层图片编辑器，支持画图工具、图层与 AI 接口。</p>"
           "<p>AI 接口: <code>POST http://127.0.0.1:48123/command</code>（JSON 指令）。</p>"));
}

// ============================================================================
// 图层操作
// ============================================================================

void MainWindow::onLayerAdd()
{
    Document before = m_doc;
    m_doc.addLayer();
    commitEdit(before, m_doc, tr("新建图层"));
}

void MainWindow::onLayerRemove(int index)
{
    if (m_doc.layerCount() <= 1) {
        m_statusBar->showMessage(tr("至少保留一个图层"), 2500);
        return;
    }
    Document before = m_doc;
    m_doc.removeLayer(index);
    commitEdit(before, m_doc, tr("删除图层"));
}

void MainWindow::onLayerDuplicate(int index)
{
    Document before = m_doc;
    m_doc.duplicateLayer(index);
    commitEdit(before, m_doc, tr("复制图层"));
}

void MainWindow::onLayerMove(int from, int to)
{
    if (to < 0 || to >= m_doc.layerCount())
        return;
    Document before = m_doc;
    m_doc.moveLayer(from, to);
    commitEdit(before, m_doc, tr("移动图层"));
}

void MainWindow::onLayerSetActive(int index)
{
    m_doc.setActiveIndex(index);
    m_layerPanel->refresh(m_doc);
}

void MainWindow::onLayerSetVisible(int index, bool visible)
{
    m_doc.setLayerVisible(index, visible);
    m_canvas->invalidate();
}

void MainWindow::onLayerSetOpacity(int index, int opacity)
{
    m_doc.setLayerOpacity(index, opacity);
    m_canvas->invalidate();
}

void MainWindow::onLayerSetBlend(int index, Blend blend)
{
    m_doc.setLayerBlend(index, blend);
    m_canvas->invalidate();
}

void MainWindow::onLayerRename(int index, const QString &name)
{
    m_doc.renameLayer(index, name);
}

// ============================================================================
// 画布信号
// ============================================================================

void MainWindow::onFileDropped(const QString &filePath)
{
    if (maybeSave())
        loadFile(filePath);
}

void MainWindow::onZoomChanged(qreal zoom) { if (m_zoomLabel) m_zoomLabel->setText(tr("%1%").arg(int(zoom * 100 + 0.5))); }

void MainWindow::onCursorMoved(const QPoint &imagePos, bool valid)
{
    if (!m_posLabel) return;
    m_posLabel->setText(valid ? tr("X: %1  Y: %2").arg(imagePos.x()).arg(imagePos.y()) : QString());
}

void MainWindow::onCropRectChanged(bool hasRect)
{
    if (hasRect)
        m_statusBar->showMessage(tr("裁剪框已确定，按 Enter 应用，按 Esc 取消"), 5000);
}

void MainWindow::onMaskChanged(bool hasMask)
{
    if (hasMask)
        m_statusBar->showMessage(tr("已标记待消除区域，执行 Ctrl+W 消除水印"), 3000);
}

// ============================================================================
// 窗口事件
// ============================================================================

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) event->accept();
    else event->ignore();
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *md = event->mimeData();
    if (md->hasUrls() && md->urls().size() == 1 && md->urls().first().isLocalFile())
        event->acceptProposedAction();
    else
        event->ignore();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *md = event->mimeData();
    if (md->hasUrls() && md->urls().size() == 1 && md->urls().first().isLocalFile()) {
        event->acceptProposedAction();
        onFileDropped(md->urls().first().toLocalFile());
        return;
    }
    event->ignore();
}

// ============================================================================
// AI 指令处理（主线程，可撤销）
// ============================================================================

static const char *blendName(Blend b) {
    switch (b) {
    case Blend::Multiply: return "multiply";
    case Blend::Screen:   return "screen";
    case Blend::Normal:   break;
    }
    return "normal";
}
static Blend blendFromName(const QString &s) {
    if (s == "multiply") return Blend::Multiply;
    if (s == "screen") return Blend::Screen;
    return Blend::Normal;
}

static QColor jsonColor(const QJsonValue &v, const QColor &def)
{
    if (!v.isString()) return def;
    QColor c(QString::fromUtf8(v.toString().toUtf8()));
    return c.isValid() ? c : def;
}

QJsonObject MainWindow::handleCommand(const QJsonObject &req)
{
    const QString cmd = req.value("command").toString();
    QJsonObject ok;
    ok["ok"] = true;

    auto layerIndex = [&](int def) {
        return req.contains("layer") ? req.value("layer").toInt(def) : def;
    };

    if (cmd == "status") {
        QJsonArray arr;
        for (int i = 0; i < m_doc.layerCount(); ++i) {
            QJsonObject o;
            o["index"] = i;
            o["name"] = m_doc.layerAt(i).name;
            o["visible"] = m_doc.layerAt(i).visible;
            o["opacity"] = m_doc.layerAt(i).opacity;
            o["blend"] = QString::fromLatin1(blendName(m_doc.layerAt(i).blend));
            arr.append(o);
        }
        ok["width"] = m_doc.width();
        ok["height"] = m_doc.height();
        ok["active"] = m_doc.activeIndex();
        ok["layers"] = arr;
        return ok;
    }
    if (cmd == "snapshot") {
        QImage img = m_doc.composite();
        QByteArray ba;
        QBuffer buf(&ba);
        buf.open(QIODevice::WriteOnly);
        img.save(&buf, "PNG");
        ok["png_base64"] = QString::fromLatin1(ba.toBase64());
        return ok;
    }
    if (cmd == "new_doc") {
        newDocument(req.value("width").toInt(900), req.value("height").toInt(600),
                    jsonColor(req.value("fill"), Qt::white));
        m_undoStack->clear();
        setCurrentFile(QString());
        return ok;
    }
    if (cmd == "add_layer") {
        Document before = m_doc;
        m_doc.addLayer(req.value("name").toString(), jsonColor(req.value("fill"), Qt::transparent));
        commitEdit(before, m_doc, tr("新建图层"));
        ok["active"] = m_doc.activeIndex();
        return ok;
    }
    if (cmd == "select_layer") {
        m_doc.setActiveIndex(req.value("index").toInt(m_doc.activeIndex()));
        m_layerPanel->refresh(m_doc);
        return ok;
    }
    if (cmd == "set_layer") {
        const int idx = req.value("index").toInt(m_doc.activeIndex());
        if (idx < 0 || idx >= m_doc.layerCount()) { ok["ok"] = false; ok["error"] = "bad index"; return ok; }
        if (req.contains("visible")) m_doc.setLayerVisible(idx, req.value("visible").toBool());
        if (req.contains("opacity")) m_doc.setLayerOpacity(idx, req.value("opacity").toInt(255));
        if (req.contains("blend")) m_doc.setLayerBlend(idx, blendFromName(req.value("blend").toString()));
        if (req.contains("name")) m_doc.renameLayer(idx, req.value("name").toString());
        m_canvas->invalidate();
        m_layerPanel->refresh(m_doc);
        return ok;
    }
    if (cmd == "move_layer") {
        Document before = m_doc;
        m_doc.moveLayer(req.value("index").toInt(), req.value("to").toInt());
        commitEdit(before, m_doc, tr("移动图层"));
        return ok;
    }
    if (cmd == "delete_layer") {
        Document before = m_doc;
        m_doc.removeLayer(req.value("index").toInt(m_doc.activeIndex()));
        commitEdit(before, m_doc, tr("删除图层"));
        return ok;
    }
    if (cmd == "clear") {
        const int idx = layerIndex(m_doc.activeIndex());
        Document before = m_doc;
        m_doc.clearLayer(idx);
        commitEdit(before, m_doc, tr("清空图层"));
        return ok;
    }
    if (cmd == "undo") { m_undoStack->undo(); return ok; }
    if (cmd == "redo") { m_undoStack->redo(); return ok; }
    if (cmd == "export") {
        const QString path = req.value("path").toString();
        if (path.isEmpty()) { ok["ok"] = false; ok["error"] = "need path"; return ok; }
        ok["saved"] = writeFile(path);
        return ok;
    }
    if (cmd == "procedural") {
        const int idx = layerIndex(m_doc.activeIndex());
        if (idx < 0 || idx >= m_doc.layerCount()) { ok["ok"] = false; ok["error"] = "bad layer"; return ok; }
        QImage img = m_doc.layerAt(idx).image.copy();
        const bool done = Painters::generate(img, req.value("kind").toString("plasma"), req);
        if (!done) { ok["ok"] = false; ok["error"] = "unknown kind"; return ok; }
        Document before = m_doc;
        m_doc.layerAt(idx).image = img;
        commitEdit(before, m_doc, tr("程序化作画"));
        return ok;
    }
    if (cmd == "draw") {
        const int idx = layerIndex(m_doc.activeIndex());
        if (idx < 0 || idx >= m_doc.layerCount()) { ok["ok"] = false; ok["error"] = "bad layer"; return ok; }
        const QString type = req.value("type").toString();
        const QColor color = jsonColor(req.value("color"), Qt::black);
        const int width = qMax(1, req.value("width").toInt(2));
        const QJsonObject shape = req.value("shape").toObject();
        QImage img = m_doc.layerAt(idx).image.copy();
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        if (type == "fill") {
            p.end();
            const bool r = ImageOps::floodFill(img, shape.value("x").toInt(),
                                               shape.value("y").toInt(), color,
                                               shape.value("tolerance").toInt(32), false);
            Document before = m_doc; m_doc.layerAt(idx).image = img;
            commitEdit(before, m_doc, tr("填充"));
            ok["filled"] = r; return ok;
        }
        QPen pen(color, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        if (type == "line") {
            p.drawLine(QPointF(shape.value("x1").toDouble(), shape.value("y1").toDouble()),
                       QPointF(shape.value("x2").toDouble(), shape.value("y2").toDouble()));
        } else if (type == "rect") {
            p.drawRect(QRectF(shape.value("x").toDouble(), shape.value("y").toDouble(),
                              shape.value("w").toDouble(), shape.value("h").toDouble()));
        } else if (type == "ellipse") {
            p.drawEllipse(QRectF(shape.value("x").toDouble(), shape.value("y").toDouble(),
                                 shape.value("w").toDouble(), shape.value("h").toDouble()));
        } else if (type == "brush" || type == "pencil") {
            p.setRenderHint(QPainter::Antialiasing, type == "brush");
            const QJsonArray pts = shape.value("points").toArray();
            QPointF prev;
            for (int i = 0; i < pts.size(); ++i) {
                const QJsonArray xy = pts[i].toArray();
                if (xy.size() < 2) continue;
                QPointF cur(xy[0].toDouble(), xy[1].toDouble());
                if (i > 0) p.drawLine(prev, cur);
                else p.drawPoint(cur);
                prev = cur;
            }
        } else if (type == "text") {
            QFont f = p.font();
            f.setPixelSize(qMax(6, shape.value("px").toInt(24)));
            p.setFont(f);
            p.drawText(QPointF(shape.value("x").toDouble(), shape.value("y").toDouble()),
                       shape.value("text").toString());
        } else {
            p.end();
            ok["ok"] = false; ok["error"] = "unknown draw type"; return ok;
        }
        p.end();
        Document before = m_doc; m_doc.layerAt(idx).image = img;
        commitEdit(before, m_doc, tr("AI 绘制"));
        return ok;
    }

    ok["ok"] = false;
    ok["error"] = "unknown command: " + cmd;
    return ok;
}
