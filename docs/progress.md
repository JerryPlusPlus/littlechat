# 当前进度

更新时间：2026-10-22（Asia/Shanghai）。阶段：**P1 工程整理、P2 Linux 网络骨架已验收；P3 认证与消息持久化待实现。**

## 2026-10-21：P1 历史记录

以下环境与功能状态记录于 P1，当日尚未在沙箱外验证 WSL；当前结论见下方 P2。

### 开始本轮时的事实

- 工作区已有未提交的 Qt 客户端源码、资源和文档，全部保留，不重置 Git。
- 源码已从旧 `littlechat/` 渐进迁移到 `src/`，已有三个独立测试套件。
- 已实现本地文字消息、草稿、示例回复、会话搜索/置顶/未读和原生窗口适配。
- 消息仍同步保存 JSON；没有 QTcpSocket、Qt SQL SQLite、Protobuf 或服务端。
- 上一轮三组 CTest 已通过；本轮修改后会重新验证，不能将旧结果作为新代码结果。

### 执行前环境检查

| 项目 | 当前结果 |
| --- | --- |
| Windows | 当前执行主机为 Windows 11 |
| Qt | `C:/Qt/6.8.3`，MinGW 与 MSVC 2022 套件已安装 |
| Qt Network/SQL | 两套件均存在 CMake 包；MSVC `qsqlite.dll` 已存在 |
| 编译器 | MinGW GCC 13.1.0；Visual Studio 2022 Enterprise MSVC 14.44.35207 已安装 |
| 构建工具 | CMake、Qt Tools Ninja 可用 |
| WSL | `wsl --list --verbose` 与 `wsl --status` 返回 `Wsl/E_ACCESSDENIED`，当前执行环境无法访问；不能据此判断宿主是否安装 Ubuntu |
| Docker | CLI/Compose 存在，Compose v5.3.1；无法连接 `dockerDesktopLinuxEngine` 命名管道 |
| protoc / muduo / MySQL / Redis | 当前可访问的执行环境未确认可用，不标记为已安装或已验证 |

### 本轮完成的改动

- 保留所有既有源码和交互，按 app / ui / controllers / models / storage / platform 分类；标题栏按钮、会话代理、消息气泡和数据模型已独立。
- 28 个 SVG 和公共 QSS 放入 `resources`，通过 Qt Resource System 嵌入，静态库显式注册资源。
- 新增 `client/CMakeLists.txt` 独立入口；根 CMake 选择组件，强制最高 C++17、关闭语言扩展。服务端尚未实现时明确拒绝构建，不生成假目标。
- `ChatController` 接管本地消息发送和延迟模拟回复的业务协调；MainWindow 保留显示/交互。真实 Service/Network/SQLite 层仍待 P4。
- 移除标题栏 QQ 标记和企鹅轮廓，替换为原创聊天标记；示例技术群头像为通用代码图形。旧用户 JSON 不被覆盖或迁移。
- 原先嵌套的 `littlechat/.vs` 缓存已保留在 `build/editor-cache/legacy-littlechat/.vs`；根目录 `.vs` 不动。旧的空源码目录已移除。
- 新增 AGENTS、总体架构、协议、数据库、六阶段路线、依赖记录、性能方案和面试总结。服务端/协议目录当前只有清晰的边界说明。
- 更新构建脚本支持自动初始化 MSVC x64 环境；生成实际文件树并更新界面截图与便携版。

### 实际验证结果

均为本轮实际执行；不是预期结果，也不等于 Linux 服务端验证。

| 检查 | 命令/位置 | 实际结果 |
| --- | --- | --- |
| MinGW Release 构建、测试、部署 | `scripts/build.ps1 -Package`；Qt 6.8.3 / GCC 13.1.0 | 构建成功；chatstore / mainwindow / window_win **3/3 通过**；便携版已更新 |
| 独立客户端配置/构建 | `cmake -S client -B build/client-standalone ...`；`cmake --build build/client-standalone -j 6` | 成功，不需要任何服务端依赖 |
| 独立客户端回归 | `ctest --test-dir build/client-standalone --output-on-failure` | **3/3 通过** |
| MSVC Release | `scripts/build.ps1 -Preset x64-release`；MSVC 19.44.35228.0 / Qt 6.8.3 | 构建成功；**3/3 通过** |
| 高 DPI 原生窗口 | `QT_SCALE_FACTOR=1.5` 后运行 `tst_window_win.exe -platform windows` | `nativeSnapContract` 通过，检查窗口样式、HTMAXBUTTON、标题栏、原生最大化/还原和圆角属性 |
| C++ 标准上限 | `-DCMAKE_CXX_STANDARD=20` | 预期失败，确认错误为 `C++17 at most`；日志 `build/guard-cpp20.log` |
| Windows 服务端平台边界 | `-DLITTLECHAT_BUILD_CLIENT=OFF -DLITTLECHAT_BUILD_SERVER=ON` | 预期失败，明确要求 Ubuntu Linux；日志 `build/guard-server-platform.log` |
| 便携版脱离 Qt PATH | PATH 仅为 Windows 系统目录，以独立数据文件生成 main/chat/compact 截图 | 三次退出码均为 0；已目视检查宽屏及窄屏聊天截图 |
| 便携版一致性 | SHA256 对比构建与部署的 `littlechat.exe` | 相同：`2E05F502D25997804114F951C7E550438A5211F794F2E59F7C01A098737CC67D` |

数据测试覆盖 Unicode/多行、空消息、持久化；UI 测试覆盖搜索、草稿切换、发送/延迟回复、未读和 360/480 DIP 布局。测试日志位于各构建目录的 `Testing/Temporary/LastTest.log`，150% 原生结果位于 `build/phase1-native-150.txt`。

MSVC 首次使用默认 Debug 编译器探测时返回 **C1902 program database manager mismatch**。改用 `CMAKE_TRY_COMPILE_CONFIGURATION=Release` 后 Release 构建通过，已写入 Release 预设；这不表示 Debug/PDB 环境已修复，Debug 配置本轮未通过验收。错误含义见[微软说明](https://learn.microsoft.com/en-us/cpp/error-messages/compiler-errors-1/fatal-error-c1902?view=msvc-170)。


## 2026-10-22：P2 网络骨架已验收

### 设计与环境

沿用客户端/服务端/公共协议的模块化单体边界，不移动或覆盖已有客户端代码。先完成 Qt 无关协议、muduo 诊断收发和有界任务池；认证和可靠聊天另行验收。

审批允许的沙箱外检查确认 VSRemote 为 Ubuntu 24.04.3 LTS，GCC 13.3.0、CMake 4.1.5、Ninja、Protobuf 3.21.12、Boost 1.83 可用。之前 Wsl/E_ACCESSDENIED 是沙箱内限制，不能继续据此声称宿主没有 Linux。没有安装/配置 WSL、启动系统服务或更改安全设置。

muduo v2.0.2 / GoogleTest v1.15.2 已获取到 build/_deps，并固定完整 commit；精确版本、许可证和唯一 muduo 补丁 SHA256 见 [dependencies.md](dependencies.md)。Protobuf 使用当前 Ubuntu 已有的 3.21.12 补丁包作为开发基线，protoc/runtime 精确一致，不是生产版本建议。

### 实现

- protocol/chat.proto：Envelope、HELLO、PING/PONG、诊断 ECHO 与错误。认证/聊天枚举预留，未实现业务 body，不发送持久化 ACK。
- 有界长度前缀编解码：5..1MiB，版本/flags/长度校验、半包/粘包/多帧，Protobuf 字节与递归限制。
- muduo one loop per thread、默认 epoll、loopback 监听、连接上下文/限额、输入/输出积压、握手/半包/空闲超时、每轮 64 帧预算。
- std::thread/mutex/condition_variable/atomic 有界任务池：非阻塞提交、Full/Stopped、排空/取消等待任务、异常隔离、owner join。诊断请求保持轻量 IO 处理；数据库/哈希尚未接入。
- signalfd 管理 SIGINT/SIGTERM，停止监听、IO 建连屏障、连接排空与延迟强制关闭、内部删除之后清理注册表，再退出主 loop、join IO 线程。
- 根 CMake 增加组件选择，client/server/protocol 可独立配置，新增 Linux Debug/Release/Sanitizer 预设，保持 C++17、关闭扩展。
- 文档同步实际能力和运行命令；既有 Qt UI、用户 JSON 与便携版保持可用。

### 缺陷与修复

早期集成测试真实触发 muduo handleClose 状态断言。stopRead 后 forceClose 引起 pinned EPollPoller 对已删除 Channel 再次 disableAll 的重新注册/重复关闭路径。修复为停服期间继续消费输入/EOF，并明确 listener、建连任务、内部 removeConnection 和应用注册表的销毁顺序。

上游没有公开停止监听接口，仅新增受控 stopAccepting 补丁，在主 EventLoop pending-functor 阶段调用。没有删除失败用例；新增关闭期间并发建连回归，并在 ASan/UBSan 下重复验证。

### 实际构建与测试

本轮均实际执行，最终源码包含最大 Envelope 后跟一帧的边界回归。Linux 各配置分别有协议 10、线程池 8、TCP 集成 10 个 GoogleTest 用例，合计 **28**；CTest 将它们登记为三组。

| 验证 | 命令 | 实际结果 |
| --- | --- | --- |
| 固定依赖获取/重复检查 | bash scripts/fetch-linux-deps.sh | 成功；无额外上游 diff |
| 独立协议构建/测试（开发中） | cmake -S protocol -B build/linux-protocol ...；cmake --build；ctest --test-dir | 成功，早期 9 用例；最终 10 用例由下列根配置覆盖 |
| 独立服务端构建/测试（开发中） | cmake -S server -B build/linux-server ...；cmake --build；ctest --test-dir | 修复后成功；最终源码由下列根配置覆盖 |
| Linux Debug | cmake --preset linux-debug；cmake --build --preset linux-debug -j 4；ctest --preset linux-debug | **3/3 通过，28 用例** |
| Linux Release | cmake --preset linux-release；cmake --build --preset linux-release -j 4；ctest --preset linux-release | **3/3 通过，28 用例** |
| ASan + UBSan | cmake --preset linux-asan；cmake --build --preset linux-asan -j 2；下方环境变量下 ctest | **3/3 通过，28 用例**，未报告检测错误 |
| 关闭/断连重复回归 | Sanitizer 集成程序 --gtest_repeat=10，筛选下方三项 | 10 轮 × 3 项通过，共 30 次；日志 build/linux-asan/lifecycle-repeat.log |
| Windows Qt 回归 | scripts/build.ps1，Qt 6.8.3 / MinGW GCC 13.1.0 | 构建成功，chatstore / mainwindow / window_win **3/3 通过** |

Sanitizer 命令：

```bash
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 ctest --preset linux-asan

ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ./build/linux-asan/server/tst_server_integration ./build/linux-asan/bin/littlechat_server \
  --gtest_repeat=10 \
  --gtest_filter='NetworkTest.HandlesPeerDisconnectWithIncompleteFrame:NetworkTest.SigtermDuringConnectionChurn:LimitedNetworkTest.EnforcesConnectionLimit'
```

日志在各配置 Testing/Temporary/LastTest.log。Sanitizer 覆盖自有代码和源码构建的 muduo；系统 Protobuf/Boost、GoogleTest 未单独启用插桩；未运行 TSan。16 客户端 × 10 ECHO 仅是隔离与正确性验证，不是吞吐或连接容量成绩。

### 当前边界与下一步

P1/P2 通过；P3 尚未开始。Qt 仍为本地演示，没有连接本服务端；MySQL 消息事务、认证、离线同步、重试幂等、SQLite、Redis 和压测均未完成。日志目前复用 muduo stdout，滚动配置待完善。

只读检查发现 VSRemote 已有 mysql/mysqld **8.0.46-0ubuntu0.24.04.4** 及本地 socket，但缺少 mysql.h / libmysqlclient.so、argon2.h / libargon2.so 开发文件。本轮未连接既有数据库，也未修改数据或启动服务。P3 先准备可复现的 C++17 客户端/密码哈希依赖和隔离数据库配置，再做迁移、连接池和认证；不把数据库命令存在当作业务持久化已验证。

perf、TCP 压测工具与 Docker Compose 部署仍未执行，性能指标均为未测。MSVC Debug/PDB 的历史 C1902 问题也未在本轮修复。
