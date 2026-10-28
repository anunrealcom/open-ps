#pragma once

#include <QWidget>
#include <QString>
#include <QRect>
#include <QJsonObject>

#include "Document.h"
#include "AiServer.h"
#include "ImageCanvas.h"

class LayerPanel;
class ToolBox;
class ToolOptions;
class TitleBar;
class QUndoStack;
class QLabel;
class QToolBar;
class QMenuBar;
class QStatusBar;
class QSlider;
class QVBoxLayout;
class QGraphicsDropShadowEffect;
class QActionGroup;

// ============================================================================
// MainWindow: 无边框圆角主窗口（QQ 风格）。整合多图层文档、画图工具、
// 图层面板、撤销栈，并作为 AiController 承接本地 HTTP+JSON 指令。
// ============================================================================
class MainWindow : public QWidget, public AiController
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool openPath(const QString &path);

    // AiController
    QJsonObject handleCommand(const QJsonObject &request) override;

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    // 文件
    void newFile();
    void openFile();
    void importImageLayer();
    void clearActiveLayer();
    void clearAllLayers();
    bool saveFile();
    bool saveFileAs();
    // 色彩 / 滤镜（作用于活动图层）
    void showAdjustDialog();
    void applyGrayscale();
    void applyInvert();
    void applyAutoLevels();
    void showBlurDialog();
    void showSharpenDialog();
    // 几何（作用于整个文档）
    void showRotateDialog();
    void rotate90CW();
    void rotate90CCW();
    void rotate180();
    void flipHorizontal();
    void flipVertical();
    void applyCrop();
    void showResizeDialog();
    void showCanvasResizeDialog();
    // 水印消除
    void applyInpaint();
    void startAutoInpaint();          // 框选去水印：下一步拖框即自动消除
    void onMaskRectFinished();
    // 画笔属性 / 文字 / 取色
    void pickPenColor();
    void setPenWidthFromSlider(int width);
    void addTextAt(const QPointF &imagePos);
    void onColorPicked(const QColor &color);
    // 视图
    void toggleCompare(bool on);
    void showAbout();
    // 图层
    void onLayerAdd();
    void onLayerRemove(int index);
    void onLayerDuplicate(int index);
    void onLayerMove(int from, int to);
    void onLayerSetActive(int index);
    void onLayerSetVisible(int index, bool visible);
    void onLayerSetOpacity(int index, int opacity);
    void onLayerSetBlend(int index, Blend blend);
    void onLayerRename(int index, const QString &name);
    // 画布信号
    void onCommitLayerEdit(int layerIndex, const QImage &beforeImage, const QString &desc);
    void onFileDropped(const QString &filePath);
    void onZoomChanged(qreal zoom);
    void onCursorMoved(const QPoint &imagePos, bool valid);
    void onCropRectChanged(bool hasRect);
    void onMaskChanged(bool hasMask);

private:
    void setupFrameless();
    void buildUi();
    void buildMenus();
    void buildToolBar();
    void buildStatusBar();
    void buildConnections();
    void applyFrameMode();
    void toggleMaximize();
    Qt::Edges resizeEdgesAt(const QPoint &pos) const;

    void selectTool(ImageCanvas::Tool tool);

    bool loadFile(const QString &path);
    bool maybeSave();
    bool writeFile(const QString &path);
    void newDocument(int w, int h, const QColor &bg);
    void fitBlankDocToView();   // 把默认空白文档铺满画布可视区
    void applyLayerOp(const QString &desc, const QImage &newActiveLayer); // 滤镜/修补/文字
    void commitEdit(const Document &before, const Document &after, const QString &desc);
    void applyDocument(const Document &d);
    void setCurrentFile(const QString &path);
    void updateCaption();
    void refreshActionStates();

    // ---- 结构 ----
    QWidget *m_frame = nullptr;
    TitleBar *m_titleBar = nullptr;
    QMenuBar *m_menuBar = nullptr;
    QToolBar *m_toolBar = nullptr;
    ImageCanvas *m_canvas = nullptr;
    LayerPanel *m_layerPanel = nullptr;
    ToolBox *m_toolBox = nullptr;
    ToolOptions *m_toolOptions = nullptr;
    QStatusBar *m_statusBar = nullptr;
    QVBoxLayout *m_outerLayout = nullptr;
    QGraphicsDropShadowEffect *m_shadow = nullptr;

    Document m_doc;
    QUndoStack *m_undoStack = nullptr;
    class AiServer *m_ai = nullptr;

    QString m_filePath;
    bool m_modified = false;
    bool m_maximized = false;
    bool m_initialized = false;
    bool m_pendingAutoInpaint = false;
    QRect m_normalGeom;
    int m_border = 14;

    QLabel *m_sizeLabel = nullptr;
    QLabel *m_posLabel = nullptr;
    QLabel *m_zoomLabel = nullptr;
    QSlider *m_penWidthSlider = nullptr;
    QActionGroup *m_toolGroup = nullptr;
};
