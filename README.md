# EUI-NEO 可视化编辑器Demo

这是基于 EUI-NEO 做的可视化 UI 编辑器原型，目标是实现类似 Android Studio XML 设计器 / Compose Preview 的开发体验。

当前包含两个应用：

```text
apps/eui_visual_editor/      XML / JSON 实时预览器
apps/eui_visual_designer/    可拖拽式 UI 设计器原型
```

## 功能

- 使用 EUI-NEO 作为底层 UI 框架
- 支持 XML / JSON 描述 UI
- 保存 XML 后自动刷新预览
- 可视化设计器支持组件添加、选中、拖动和属性编辑
- 可保存设计结果为 XML
- 支持从组件库直接拖入画布
- 支持右下角拖拽缩放
- 支持撤销、重做、复制和删除
- 自动打开上次保存的设计文件

## 支持的组件

```text
Rect
Text
Button
Image
Panel
Card
Row
Column
CardSlider
```

## 使用方式

把本仓库的 `apps/` 目录复制到 EUI-NEO 项目根目录：

```text
EUI-NEO/
└─ apps/
   ├─ eui_visual_editor/
   └─ eui_visual_designer/
```

然后在 EUI-NEO 根目录构建：

```sh
cmake -S . -B build -DEUI_BUILD_USER_APPS=ON
cmake --build build --target eui_visual_designer
cmake --build build --target eui_visual_editor
```

运行设计器：

```sh
./build/eui_visual_designer
```

运行预览器：

```sh
./build/eui_visual_editor
```

## 说明

`eui_visual_designer` 用来可视化创建和编辑 UI。可以把左侧组件直接拖入画布，也可以先点击组件，再点击画布完成放置。选中组件后可以拖动位置、拖动右下角手柄调整尺寸，并在右侧修改属性。

点击 `Save XML` 后会保存到：

```text
apps/eui_visual_designer/designed.ui.xml
```

`eui_visual_editor` 会优先读取这个 XML 文件并实时预览；如果该文件不存在，则读取自带示例：

```text
apps/eui_visual_editor/examples/basic.ui.xml
```

## 当前阶段

已经经过测试：

- 组件库
- 画布
- 节点选中
- 拖动移动
- 拖拽缩放
- 属性面板
- 撤销 / 重做
- 复制 / 删除
- XML 保存
- XML 自动加载
- 实时预览

后续加入：

- 图层树
- 对齐辅助线
- 多选与组合
- 插件组件注册
- 第三方组件库支持
- 逻辑代码低代码化
- 纯低代码开发逻辑封装
