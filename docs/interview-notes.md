# 校招面试技术总结

只描述代码和实验能支撑的能力；实际命令与结果见 [progress.md](progress.md)。

## 当前可以演示

- Windows Qt 6 本地聊天界面：Controller 分层、资源系统、SVG/QSS、草稿/未读/JSON、Snap 的 HTMAXBUTTON 与 Win32/DPI 测试。
- Ubuntu muduo one loop per thread：真实 TCP、连接上下文、epoll、HELLO/PING/ECHO 和多客户端隔离。
- 长度前缀协议：字节序、半包/粘包、多帧预算、恶意长度、最大包、Protobuf 递归限制。
- C++11 有界线程池：任务所有权、mutex/CV 等待谓词、非阻塞满队列拒绝、异常隔离、排空/取消、owner join。
- 生命周期控制：weak_ptr、IO 线程归属、signalfd、计时器、建连屏障与停服回调顺序。
- CMake 独立模块、固定依赖/补丁、GoogleTest/CTest、Linux Debug/Release/ASan+UBSan 与 Windows 回归。

这些是功能与正确性验证；16 客户端测试和重复竞态回归不代表高并发容量或压测成绩。

## 一次真实关闭缺陷

早期 TCP 集成测试在断连/停服时触发 muduo TcpConnection::handleClose 状态断言。原因是先 stopRead，再 forceClose：该版本 EPollPoller 在 Channel 已删除时再次 disableAll，会尝试重新 ADD 空事件，随后 HUP 导致重复关闭路径。

修复保持读事件直到关闭，停服期间消费输入和 EOF；再调整 listener、connectEstablished、removeConnection 与应用注册表的清理顺序。单纯延迟析构或删掉失败测试不能证明修复。新增关闭期间并发建连回归，连同断半包/连接限额，在 ASan/UBSan 下重复 10 轮通过；不是穷尽所有时序的证明。

另外，muduo v2.0.2 没有公开停止监听 API。最小补丁增加 stopAccepting，必须在主 loop pending-functor 阶段调用。项目同时锁定 commit 和补丁精确内容，能解释为什么改、改了哪里、如何回归。

## 应从代码解释的问题

| 主题 | 讨论要点 |
| --- | --- |
| Reactor | EventLoop/Channel/Poller；内核就绪不是完整业务消息；连接固定 IO loop |
| 公平与背压 | 64 帧预算后的 queueInLoop；输入/输出与任务队列分别有界；阈值有瞬时余量 |
| 超时 | steady_clock；空闲和半包等待不同，滴入字节不能无限延长半包 |
| 任务队列 | pending 不含 running；Full 的调用方处理；取消等待任务不强杀运行任务 |
| 生命周期 | 最后声明的 TcpServer 先析构/join；先停止接收、等屏障和内部清理，再销毁状态 |
| 测试 | 每字节边界、最大包后续帧、并发隔离、进程级信号；ASan 不替代竞态分析 |

## 尚需由后续实现支撑

MySQL 连接池/RAII 租约、唯一键和事务幂等、提交后断线、可靠 ACK、离线同步游标、Argon2id、会话代次、Redis TTL、Qt SQLite/重连、perf 与 P50/P95/P99 目前仍是计划。不能宣称已实现真实用户聊天、持久化投递、5000 连接或端到端 exactly-once。

语言最高 C++17、风格以 C++11 为主。当前少量泛型 lambda 属于 C++14，用于列表比较；线程池使用 std::thread/mutex/condition_variable/atomic，无协程和模板元编程。
