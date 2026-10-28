# PictureTool

基于 **Qt 6 / C++17** 的轻量级图片编辑器，纯 Qt Widgets 实现，无第三方依赖。

## 功能一览

| 分类 | 功能 |
|------|------|
| 文件 | 打开 / 保存 / 另存为（PNG、JPG、BMP、TIFF、WebP）、拖拽图片到窗口打开、按 EXIF 自动转正 |
| 编辑 | 无限撤销 / 重做 |
| 色彩 | 亮度 / 对比度 / 饱和度 / 色温（实时预览）、灰度、反色、自动色阶 |
| 滤镜 | 高斯模糊、锐化（Unsharp Mask，实时预览） |
| 几何 | 任意角度旋转（可选背景色）、90°/180° 快捷旋转、水平/垂直翻转、裁剪（三分线辅助框）、调整图像大小、调整画布大小（九宫格锚点） |
| 绘图标注 | 画笔、直线、矩形、椭圆（颜色 / 粗细可调）、文字（字体 / 颜色可选，支持多行） |
| 水印消除 | 遮罩画笔 / 矩形遮罩标记区域 + 多尺度扩散修补算法（内容感知填充） |
| 视图 | 滚轮缩放（Ctrl+滚轮以光标为中心）、平移、适合窗口、100%、按住对比原图、状态栏实时显示坐标 / 缩放比例 |
| 界面 | QQ 风格浅色主题、无边框圆角窗口 + 柔和投影、自定义标题栏（logo + 最小化/最大化/关闭）、菜单/工具栏全矢量图标、可拖拽边缘改变大小 |

## 常用快捷键

- `Ctrl+O` 打开，`Ctrl+S` 保存，`Ctrl+Shift+S` 另存为
- `Ctrl+M` 亮度/对比度，`Ctrl+B` 模糊，`Ctrl+Alt+S` 锐化
- `Ctrl+Z` / `Ctrl+Y` 撤销 / 重做
- `W` / `E` / `Shift+W` 遮罩画笔 / 橡皮 / 矩形遮罩，`Ctrl+W` 执行消除
- `P` 画笔，`L` 直线，`U` 矩形，`O` 椭圆，`T` 文字
- 裁剪框确认后按 `Enter` 应用、`Esc` 取消
- `Ctrl+0` 适合窗口，`Ctrl+1` 实际大小

## 构建方法

环境要求：Qt 6.3+（含 Widgets 模块）、CMake 3.16+、C++17 编译器（MSVC 2019+ / MinGW 11+ / clang / gcc）。

### 命令行

```bash
cd picturetool
cmake -B build -DCMAKE_PREFIX_PATH="C:/Qt/6.x.x/msvc2019_64"   # 按实际 Qt 安装路径修改
cmake --build build --config Release
```

可执行文件位于 `build/Release/PictureTool.exe`（Windows）或 `build/PictureTool`（macOS/Linux）。

### Qt Creator

直接用 Qt Creator 打开项目根目录的 `CMakeLists.txt`，选择 Kit 后点击运行即可。

### MSVC 编译注意

CMakeLists 已为 MSVC 开启 `/utf-8`（源码为 UTF-8 编码，含中文界面文本）。

## 源码结构

```
picturetool/
├── CMakeLists.txt
├── README.md
├── resources/
│   ├── logo.png          # 应用 logo（标题栏 / 窗口图标）
│   ├── app.ico           # Windows 可执行文件图标（多尺寸）
│   ├── app.rc            # 嵌入 .ico 的 Win32 资源脚本
│   └── resources.qrc     # Qt 资源（打包 logo.png）
└── src/
    ├── main.cpp           # 程序入口：应用主题 / 窗口图标 / 高 DPI
    ├── MainWindow.h/.cpp  # 无边框圆角主窗口：菜单/工具栏/撤销栈/功能调度
    ├── TitleBar.h/.cpp    # 自定义标题栏：logo+标题+窗口按钮，拖拽移动
    ├── Theme.h/.cpp       # QQ 风格调色板与全局样式表
    ├── IconFactory.h/.cpp # 矢量绘制的扁平图标集（菜单/工具栏/标题栏按钮）
    ├── ImageCanvas.h/.cpp # 画布：缩放平移、裁剪/遮罩/绘图工具交互
    ├── ImageOps.h/.cpp    # 图像算法：调整/滤镜/几何变换/修补（无 UI 依赖）
    ├── dialogs.h/.cpp     # 对话框：带实时预览的调整/模糊/锐化/旋转等
    └── Commands.h         # 撤销命令（QUndoCommand 封装）
```

## 关于水印消除算法

采用多尺度（图像金字塔）+ Gauss-Seidel 迭代求解 Laplace 方程的扩散式修补：以遮罩外的像素为边界条件，对遮罩区域做平滑内容感知填充。适合消除背景较平滑的水印（纯色、渐变、天空、皮肤等）；对复杂纹理区域效果有限，后续可扩展为 patch-match 纹理合成。
