#include "MainWindow.h"
#include "Theme.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    // 高分屏缩放策略必须在 QApplication 构造之前设置
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    QApplication::setApplicationName("PictureTool");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("Yihuantech");

    // Fusion 作为基础风格，配合样式表得到统一的 QQ 观感
    app.setStyle(QStyleFactory::create("Fusion"));
    app.setStyleSheet(Theme::styleSheet(14));
    app.setWindowIcon(QIcon(":/logo.png"));

    MainWindow win;
    win.show();

    // 支持命令行直接打开图片: PictureTool.exe D:\photos\1.png
    const QStringList args = app.arguments();
    if (args.size() > 1 && QFile::exists(args.at(1)))
        win.openPath(args.at(1));

    return app.exec();
}
