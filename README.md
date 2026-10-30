# littlechat · 轻聊

面向 Linux C++ 服务端校招展示的即时通信项目：Windows Qt 6 客户端 + Ubuntu muduo 模块化单体服务端。界面借鉴现代聊天软件的通用三栏布局。


## Windows 11 窗口行为

- 使用不透明的原生 HWND，保留 `WS_CAPTION / WS_THICKFRAME / WS_MAXIMIZEBOX`，通过 `WM_NCCALCSIZE` 移除系统标题栏。
- 自定义最大化按钮的 `WM_NCHITTEST` 返回 `HTMAXBUTTON`；非客户区悬停交给 Windows 处理，提供原生 **Snap Layout** 菜单，而不是自行画一个菜单。
- 支持拖动标题栏、双击最大化 / 还原、八方向缩放、最小化、任务栏工作区约束和系统贴靠。
- `DWMWA_WINDOW_CORNER_PREFERENCE = DWMWCP_ROUND` 提供普通窗口圆角，DWM 绘制系统阴影；没有透明叠层、区域裁剪或假阴影窗口。
- 按 Windows 11 系统规范，最大化或贴靠后使用直角，退出后恢复圆角。Snap 菜单是否显示受系统“贴靠窗口”设置控制。
- 原生命中测试按当前窗口 DPI 换算，支持高分屏及负屏幕坐标。
- 最小尺寸 **360 × 420 DIP**。宽度小于 760 DIP 时，自动切换为会话列表 / 聊天页单面板，用返回按钮切换，适合较窄的分屏区域。

原生行为依据：[微软 Snap Layout 文档](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu)、[DWM 圆角文档](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-rounded-corners)。


## 代码结构

```text
src/
  app/                      应用入口、命令行与截图
  ui/windows/               主窗口与页面交互
  ui/widgets/               标题栏按钮、会话代理、消息气泡
  ui/graphics/              图标渲染、头像绘制和资源注册
  core/models/              消息、会话数据结构
  core/controllers/         消息交互协调（当前仍是本地演示）
  core/storage/             会话操作与 JSON 持久化
  platform/window/          圆角、Snap Layout 与原生窗口行为
resources/
  resources.qrc             Qt 资源清单
  icons/                    SVG 图标
  images/                   图片预留目录
  styles/light.qss          公共界面样式
client/CMakeLists.txt        独立 Qt 客户端构建入口
server/                     muduo 诊断服务、有界任务池与 TCP 集成测试
protocol/                   chat.proto、帧编解码与 GoogleTest
cmake/                      Linux 依赖与 Sanitizer 构建选项
tests/                      数据、界面、Windows 窗口测试
scripts/                    构建部署、固定 Linux 依赖与文件树生成
docs/                       架构、文件树和截图
build/                      构建与便携版产物（不提交 Git）
```
