# 公共协议

P2 已实现 Qt / muduo / 数据库无关的 C++17 静态库 `littlechat_protocol`，包含 `chat.proto`、有界长度前缀编解码和 Protobuf 请求校验。

```text
protocol/
  chat.proto
  include/littlechat/protocol/frame.h
  include/littlechat/protocol/envelope.h
  src/frame.cpp
  src/envelope.cpp
  tests/tst_protocol.cpp
  CMakeLists.txt
```

线格式：`u32_be frame_length | u16_be version=1 | u16_be flags=0 | Envelope`。长度不含自身 4B，合法范围 5..1MiB。文本上限 16KiB；Protobuf 解析总量和递归深度有界。decodeFrame 每次解析一帧，返回 Complete/NeedMore/Invalid 与 consumed，调用方管理输入缓冲和剩余尾部。

现阶段实现 HELLO、PING/PONG、ECHO；认证与消息 ACK 的枚举只作预留，不代表业务已完成。字段编号、错误处理与未来投递语义见 [协议文档](../docs/protocol.md)。

生成器和 runtime 当前固定到 Ubuntu 已有的 Protobuf 3.21.12；CMake 验证精确匹配。生成的 chat.pb.* 留在构建目录，不提交另一套预生成文件。版本来源和后续升级边界见 [依赖记录](../docs/dependencies.md)。

独立构建（Ubuntu，先准备依赖）：

```bash
bash scripts/fetch-linux-deps.sh
cmake -S protocol -B build/protocol-standalone -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build/protocol-standalone -j 4
ctest --test-dir build/protocol-standalone --output-on-failure
```

Windows Qt 演示不依赖此模块。P4 接入客户端时再提供匹配的 Windows Protobuf 工具链，不能直接把 Linux 库链接到 Windows。
