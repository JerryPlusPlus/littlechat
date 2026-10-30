# Linux C++ 服务端

P2 已实现并在 Ubuntu 24.04.3 / GCC 13.3 下验证。当前程序是 **127.0.0.1 上的 TCP/Protobuf 诊断服务**，支持 HELLO、PING/PONG、ECHO。Qt 客户端还没有连接它；认证、用户间聊天和持久化 ACK 留在 P3/P4。

## 构建与运行

已有依赖见 [版本记录](../docs/dependencies.md)。在 Ubuntu 或本机 WSL 的 VSRemote 发行版内，从仓库根目录执行：

```bash
bash scripts/fetch-linux-deps.sh
cmake --preset linux-debug
cmake --build --preset linux-debug -j 4
ctest --preset linux-debug
./build/linux-debug/bin/littlechat_server --port 9988 --io-threads 2
```

按 Ctrl+C 或向该进程发送 SIGTERM 可以退出。程序固定绑定 loopback，不自动配置系统服务、开放防火墙或启动数据库。它使用二进制协议，不能通过浏览器或直接输入文本测试。

独立服务端入口不需要 Qt：

```bash
cmake -S server -B build/server-standalone -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/server-standalone -j 4
ctest --test-dir build/server-standalone --output-on-failure
```

演示真实多连接收发可以运行集成测试；测试自己创建服务器子进程及临时端口，结束后回收：

```bash
./build/linux-debug/server/tst_server_integration ./build/linux-debug/bin/littlechat_server --gtest_filter=NetworkTest.ConcurrentConnectionsReceiveOnlyTheirOwnResponses
```

所有实际用例名可用 `--gtest_list_tests` 查看；完整验收使用 CTest。

## 参数和边界

| 参数 | 默认值 | 语义 |
| --- | --- | --- |
| --port | 9988 | 1024..65535 |
| --io-threads | 2 | 0..16；0 表示连接也在主 EventLoop |
| --max-connections | 128 | 1..10000；超过时关闭新连接，数字不是性能承诺 |
| --idle-timeout-ms | 30000 | 最后一次接收数据后的空闲上限 |
| --partial-timeout-ms | 2000 | 一个不完整帧的等待上限，零碎输入不会重置 |
| --handshake-timeout-ms | 5000 | 从建立连接到 HELLO 的绝对等待上限 |

检查间隔 100ms，超时并非精确定时承诺。每帧最大 1MiB；每连接输入在读回调检查 2 × (1MiB + 4B) 的上限，输出超过 2MiB 时关闭慢读连接。muduo 单次读和已排队写入会产生瞬时余量，这些值不是进程 RSS 的硬限制。每轮最多处理 64 帧，其余完整帧通过 queueInLoop 继续。

默认 epoll，若环境设置了 `MUDUO_USE_POLL` 则启动拒绝，防止静默更换网络模型。握手后的已知业务请求返回 NOT_IMPLEMENTED，伪造身份返回 UNAUTHENTICATED，畸形帧/Protobuf/消息类型关闭连接；不会发送虚假的 PERSISTED_ACK。

## 模块与线程规则

- `src/main.cpp`：参数、RAII 文件描述符、signalfd、启动和退出。先屏蔽信号，再创建 IO 线程，由主 EventLoop 读取 SIGINT/SIGTERM。
- `src/network/`：muduo 适配、连接上下文、轻量诊断分发、限额、超时及停服。
- `src/concurrency/`：C++11 有界任务队列，trySubmit 返回 Accepted/Full/Stopped/Empty；支持 Drain 和 CancelPending、异常观察、owner 线程 join。目前诊断请求不需要阻塞任务，因此没有人为将 ECHO 放进工作池。
- `cmake/Muduo.cmake`：固定上游源码清单及版本，复用网络库；不另写 epoll 框架。
- `patches/`：唯一允许的上游修改，补充停止 acceptor 的接口。
- `tests/`：线程池 GoogleTest 与实际 POSIX TCP 集成测试。

连接状态和缓冲区只在所属 IO EventLoop 操作。跨线程注册表以短 mutex 保护，保存 weak_ptr。关闭顺序为：停止监听 → IO 建连任务屏障 → 连接 shutdown/限时 forceClose → 等待 muduo 内部删除与应用注册表清理 → 主 loop 退出 → IO 线程 join。服务对象覆盖所有回调生命期；成员声明保证 TcpServer 先于注册表和 mutex 销毁。

上游 muduo v2.0.2 没有公开停止监听接口。受控补丁增加 `stopAccepting()`，必须在主 loop 的 pending-functor 阶段调用，避开活跃 Channel 回调。获取脚本和 CMake 检查原始 commit 与精确补丁；不接受额外修改。补丁校验见依赖文档。

日志复用 muduo，当前输出时间、级别、线程、模块、请求/连接标识及诊断错误，不记录消息正文或认证信息。滚动文件与运行时日志级别配置仍待后续完善。

## 验证

Linux Debug / Release / ASan+UBSan 均为 3/3 CTest，包含协议 10、线程池 8、TCP 10 个用例。另在 Sanitizer 下对异常断连、关闭并发建连、连接限额执行 10 轮重复测试。覆盖和命令见 [进度](../docs/progress.md)。

业务数据库、重启恢复、投递可靠性和大规模性能尚未验收，后续目录按 [路线](../docs/roadmap.md) 渐进增加。
