# 项目目录与模块职责

这是已实现客户端的目录说明。系统总体方案见 [architecture.md](architecture.md)，独立客户端构建入口为 `client/CMakeLists.txt`。

完整文件树见 [project-tree.txt](project-tree.txt)，可运行 `scripts/update-tree.ps1` 重新生成。文件树省略 Git 元数据、Visual Studio 缓存和构建产物。

```text
littlechat/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── src/
│   ├── app/
│   │   └── main.cpp
│   ├── ui/
│   │   ├── windows/
│   │   │   └── mainwindow.h/.cpp
│   │   ├── widgets/
│   │   │   ├── captionbutton.h/.cpp
│   │   │   ├── conversationdelegate.h/.cpp
│   │   │   └── messagebubble.h/.cpp
│   │   └── graphics/
│   │       ├── icons.h/.cpp
│   │       └── resources.h/.cpp
│   ├── core/
│   │   ├── controllers/
│   │   │   └── chatcontroller.h/.cpp
│   │   ├── models/
│   │   │   ├── conversation.h
│   │   │   └── message.h
│   │   └── storage/
│   │       └── chatstore.h/.cpp
│   └── platform/
│       └── window/
│           └── framelesswindow.h/.cpp
├── resources/
│   ├── resources.qrc
│   ├── icons/                  # 28 个可编辑 SVG 图标
│   ├── images/                 # 预留图片目录，使用 .gitkeep 保留
│   └── styles/
│       └── light.qss
├── tests/
│   ├── tst_chatstore.cpp
│   ├── tst_mainwindow.cpp
│   └── tst_window_win.cpp
├── scripts/
│   ├── build.ps1
│   └── update-tree.ps1
├── docs/
│   ├── architecture.md
│   ├── project-tree.txt
│   └── screenshots/
└── build/                      # 生成目录，不提交 Git
    ├── mingw-release/
    ├── x64-debug/
    ├── editor-cache/           # 迁移前的嵌套 Visual Studio 缓存
    └── portable/
        ├── bin/
        ├── plugins/
        └── translations/
```

`名称.h/.cpp` 表示对应的头文件和实现文件。构建目录按实际使用的预设生成。

| 目录 | 职责 |
| --- | --- |
| `src/app` | 应用启动、命令行、字体与翻译初始化、截图入口。 |
| `src/ui/windows` | 主窗口布局、页面切换和交互事件。 |
| `src/ui/widgets` | 标题栏按钮、会话列表代理和消息气泡等复用组件。列表数据角色由代理头文件统一定义。 |
| `src/ui/graphics` | 图标着色和多分辨率渲染、程序绘制头像，以及静态库中的 Qt 资源注册。 |
| `src/core/models` | 消息和会话数据结构，不依赖界面。 |
| `src/core/controllers` | 消息交互协调、活跃会话和明确标注的本地模拟回复。 |
| `src/core/storage` | 示例会话、消息操作与 JSON 持久化，只依赖 Qt Core。 |
| `src/platform/window` | Win32 圆角、Snap Layout、窗口命中测试和最大化工作区适配。 |
| `resources` | 编译进程序的 SVG、公共 QSS 与未来的图片文件。 |
| `tests` | 数据、界面和 Windows 原生窗口三组独立测试。 |

## 构建关系

- `littlechat_core`：数据模型与本地存储静态库。
- `littlechat_ui`：界面、平台适配和 Qt 资源静态库，依赖 `littlechat_core`。
- `littlechat`：应用入口，链接 `littlechat_ui`。
- `tst_chatstore`、`tst_mainwindow`、`tst_window_win`：独立测试程序。CTest 在 Windows 上自动以原生平台运行窗口测试。

头文件统一从 `src` 开始引用，例如 `#include "core/storage/chatstore.h"`，同一目录内可使用文件名。

## 修改资源

公共界面样式放在 `resources/styles/light.qss`。图标放在 `resources/icons`，SVG 的 `currentColor` 在渲染时替换为调用方指定的颜色。头像和原创聊天标记仍由 `Icons::avatar`、`Icons::brandMark` 程序绘制；未来的外部图片放入 `resources/images`。

新增资源后，将路径登记到 `resources/resources.qrc`。重新构建即可，运行时通过 `:/styles/light.qss`、`:/icons/chat.svg` 访问，不依赖源文件位置和工作目录。

## 生成文件与用户数据

`build`、根目录的 `.vs` 不提交 Git。原先 `littlechat/.vs` 的缓存保存在 `build/editor-cache/legacy-littlechat/.vs`。日常开发应从项目根目录打开 `CMakeLists.txt`。

聊天记录仍保存在原有的应用数据目录，目录整理不会迁移或重置用户会话。以后接入聊天服务时，可在 `src/core/network` 增加连接管理与协议实现。
