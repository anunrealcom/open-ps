# PictureTool 画图 / 图层 / AI 接口 设计文档

> 本阶段目标：把「单张 QImage」升级为「多图层文档」，补齐 Windows 画图风格工具，
> 并预留一套本地 HTTP + JSON 的 AI 对外接口，内置程序化自动作画。

## 1. 总体架构

```
main.cpp
  └─ MainWindow (QWidget, 无边框圆角)
       ├─ Document        多图层文档模型（数据层，无 UI 依赖）
       ├─ ImageCanvas     画布：渲染合成结果 + 工具交互（编辑当前图层）
       ├─ LayerPanel      图层面板：增删 / 显隐 / 透明度 / 排序 / 选中
       ├─ QMenuBar/QToolBar  命令入口
       ├─ QUndoStack      基于 Document 快照的撤销/重做
       └─ AiServer        QTcpServer，localhost HTTP+JSON 指令接口
```

分层原则：
- **Document / ImageOps / Painters** 为纯数据与算法层，不依赖任何控件，便于被 UI 和 AI 接口共同复用。
- **ImageCanvas / LayerPanel** 为视图层，只读写 Document 并通过信号通知 MainWindow。
- **AiServer** 与用户操作走同一套 Document 编辑原语，保证「AI 画的」和「人手画的」行为一致。

## 2. 图层文档模型（Document / Layer）

```cpp
struct Layer {
    QString name;
    QImage  image;      // Format_ARGB32_Premultiplied，尺寸 == 文档尺寸，初始透明
    bool    visible = true;
    int     opacity   = 255;   // 0..255
};

class Document {
    int width, height;
    QVector<Layer> layers;     // 索引 0 = 最底层，末尾 = 最顶层
    int active = 0;            // 当前活动图层索引
    // 合成：自底向上叠加 visible 图层，按 opacity 混合
    QImage composite() const;
    Layer* activeLayer();
    // 结构操作：addLayer/removeLayer/moveLayer/setOpacity/setVisible/setActive/rename
    // 几何操作（作用于所有图层，保持对齐）：resize/crop/rotate/flip/resizeCanvas
};
```

关键点：
- 所有图层尺寸与文档一致；几何操作（裁剪/缩放/画布）同时改所有图层。
- 依赖 Qt **隐式共享**：`Document` 拷贝时各图层 `QImage` 只是引用计数 +1，几乎零成本；只有真正绘制到某图层才会 detach 该图层。因此「整文档快照」做撤销既简单又省内存。

## 3. 撤销模型

- 统一用 `DocCommand(before: Document, after: Document, text, apply)`。
- 绘制笔画：鼠标按下时 `before = doc` 快照，画布实时画到活动图层，松开时 `after = doc` 快照并入栈。
- 滤镜 / 结构操作：`before = doc` → 执行 → `after = doc` → 入栈。
- `QUndoStack::setUndoLimit(60)` 控制历史深度。
- apply 回调把快照写回 MainWindow 的当前 Document 并刷新画布与图层面板。

## 4. 画图工具（对齐 Windows 画图）

| 工具 | 行为 | 目标 |
|------|------|------|
| 铅笔 Pencil | 1px 逐点/连线，无抗锯齿 | 活动图层 |
| 画笔 Brush | 圆头可变粗细，抗锯齿 | 活动图层 |
| 橡皮 Eraser | CompositionMode_Clear 擦除 | 活动图层 |
| 直线 / 矩形 / 椭圆 | 拖拽预览，松开落图，可选描边色/粗细 | 活动图层 |
| 油漆桶 Fill | 洪水填充（4/8 邻域 + 容差） | 活动图层 |
| 取色器 Picker | 读取合成像素设为前景色 | 只读 |
| 文字 Text | 点击输入，字体/颜色 | 活动图层 |
| 移动视图 Pan / 缩放 | 视图操作，不改数据 | — |
| 裁剪 Crop | 文档级，改所有图层尺寸 | Document |
| 遮罩去水印 Mask* | 沿用上一阶段的 Laplace 修补，作用于活动图层 | 活动图层 |

前景色 / 线宽为全局画笔属性，工具栏可调。

## 5. 图层面板

- 列表自上而下 = 图层自顶而下（顶部为最上层）。
- 每项：可见性勾选、缩略图、名称、透明度（选中后右侧滑块）。
- 按钮：新建、删除、上移、下移、复制。
- 双击重命名。

## 6. AI 对外接口（本地 HTTP + JSON）

- `AiServer` 监听 `127.0.0.1:<port>`（默认 48123，仅本机），接收 `POST /command`，body 为 JSON，返回 JSON。
- 所有指令在主线程排队执行（通过信号转发到 MainWindow），保证与 UI 线程一致、可撤销。

### 指令集

| command | 参数 | 说明 |
|---------|------|------|
| `status` | — | 返回文档尺寸、图层列表、活动图层 |
| `snapshot` | — | 返回合成图 PNG 的 base64（让 AI“看到”画布） |
| `new_doc` | `width,height,fill?` | 新建文档 |
| `add_layer` | `name?,fill?` | 添加图层并设为活动 |
| `select_layer` | `index` | 切换活动图层 |
| `set_layer` | `index,visible?,opacity?,name?` | 修改图层属性 |
| `move_layer` | `index,to` | 重排图层 |
| `delete_layer` | `index` | 删除图层 |
| `clear` | `index?` | 清空图层（默认活动） |
| `draw` | `type,layer?,color,width,shape...` | 画 line/rect/ellipse/brush/pencil/fill/text |
| `procedural` | `kind,layer?,params...` | 程序化自动作画（见下） |
| `export` | `path,format?` | 导出合成图 |
| `undo` / `redo` | — | 撤销/重做 |

`draw` 的 shape 依类型：
- line: `{x1,y1,x2,y2}`
- rect/ellipse: `{x,y,w,h}`
- brush/pencil: `{points:[[x,y],...]}`
- fill: `{x,y,tolerance?}`
- text: `{x,y,text,px?,color}`
`color` = `"#RRGGBB"` 或 `#RRGGBBAA`。

### 程序化自动作画（内置，无需联网/模型）

`procedural.kind` 支持：
- `plasma`：正弦叠加值噪声，色彩渐变（value-noise plasma）。
- `mandelbrot` / `julia`：分形着色。
- `shapes`：随机圆/矩形/线条构成的抽象画（可设数量、色板、随机种子）。
- `flowfield`：基于噪声向量场的密集笔触（模仿油画/笔刷流）。
- `gradient`：线性/径向渐变。

统一带 `seed` 参数以便可复现；`AiServer` 侧只负责解析与排队，具体像素生成在 `Painters` 层。

### 示例

```bash
curl -s http://127.0.0.1:48123/command -d '{"command":"new_doc","width":800,"height":600}'
curl -s http://127.0.0.1:48123/command -d '{"command":"procedural","kind":"plasma","seed":7}'
curl -s http://127.0.0.1:48123/command -d '{"command":"draw","type":"rect","color":"#ffffff","width":3,"shape":{"x":50,"y":50,"w":200,"h":120}}'
curl -s http://127.0.0.1:48123/command -d '{"command":"export","path":"D:/out.png"}'
```

## 7. 分阶段落地

1. 本阶段：Document/Layer、画图工具、图层面板、AI 接口骨架 + 程序化作画。
2. 后续：图层混合模式、选区、蒙版、变换（自由变形）、云端文生图接入、撤销栈按图层增量优化。
