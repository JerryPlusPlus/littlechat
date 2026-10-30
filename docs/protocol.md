# TCP / Protobuf 协议 v1

状态：P2 已实现帧协议与诊断消息；注册、登录、可靠聊天和同步仍是 P3/P4 设计。schema 在 protocol/chat.proto，生成代码留在构建目录。

## 已实现的帧格式

```text
u32_be frame_length | u16_be version | u16_be flags | Protobuf Envelope
```

frame_length 不含自身 4 字节，包含 version/flags 和 Envelope；合法范围 5..1,048,576 字节。version=1，flags=0。先验证长度/元数据，再等待整帧；非法长度不会触发按其声明分配内存。解码函数每次提取一帧，返回 consumed；半帧不消耗输入，多帧循环处理。

接收上限为 2 × (最大帧长度 + 4)，按读回调检查；允许一个最大合法帧连同随后帧到达。输出超过 2MiB 关闭慢读连接，阈值有单次 IO 的瞬时余量。每轮最多处理 64 帧，剩余数据 queueInLoop 后续处理，即便没有新 read 事件也会继续。

默认连接握手等待 5 秒、半包 2 秒、空闲 30 秒，100ms 周期检查；使用 steady_clock。零碎字节会刷新空闲时间，但不会延长当前半包和握手的绝对等待。连接数默认 128，可配置，非性能承诺。

Protobuf 输入最多 1MiB 减 4 字节，解析递归深度 64；文本 ECHO 正文最多 16KiB。畸形 Protobuf、长度/版本/flags、未知或方向错误的消息类型会关闭连接，不记录正文。

## Envelope 已有字段

| 编号 | 字段 | 约束/含义 |
| --- | --- | --- |
| 1 | type | MessageType，非零且已知，方向与 body 一致 |
| 2 | request_id | 请求非零，响应回显，用于关联而非可靠去重 |
| 3 | message_id | 当前可空或 16B；业务阶段重试必须沿用原 UUID |
| 4 / 5 | sender_id / receiver_id | 当前诊断端必须为 0；未来发送者由认证会话确定 |
| 6 | conversation_id | 当前必须为 0；未来校验成员权限 |
| 7 | timestamp_ms | 请求不得为负；响应为服务端系统时间，仅关联时间不作超时钟 |
| 8 | error_code | 请求必须为 OK；错误响应使用稳定枚举 |
| 10..16 | oneof body | hello、hello_reply、ping、pong、echo_request、echo_response、error |

Hello client_name 为 1..64 字节。ECHO 空字符串允许，过长拒绝。请求中携带非零身份字段返回 UNAUTHENTICATED，诊断端不会信任该身份。已用字段编号不得复用；删除字段应 reserved。

## 当前诊断流程

```text
connect -> HELLO_REQUEST -> HELLO_RESPONSE(diagnostic_only=true)
        -> PING(nonce)   -> PONG(same nonce)
        -> ECHO_REQUEST  -> ECHO_RESPONSE(same text)
```

| 情况 | 处理 |
| --- | --- |
| HELLO 前发送合法 PING/ECHO | HANDSHAKE_REQUIRED |
| 重复 HELLO | BAD_STATE |
| 已握手后发送预留业务请求 | NOT_IMPLEMENTED |
| 非零身份字段 | UNAUTHENTICATED |
| 畸形结构、未知 type、错误 body、request_id=0 | 关闭连接 |
| 连接超限、积压或超时 | 关闭连接 |

REGISTER、LOGIN、LOGOUT、SEND_TEXT、PERSISTED_ACK、MESSAGE_PUSH、DELIVERY_ACK、READ_ACK、SYNC_EVENTS 的枚举已预留，但没有完整业务 body/处理器。特别是不会返回 PERSISTED_ACK。它们还不是稳定的业务 API；实现时补齐 schema 和版本测试。

## P3/P4 待实现：可靠消息

```text
local_pending -> sending -> persisted -> delivered -> read
                     \-> failed -> retry using SAME message_id
```

- persisted：MySQL 事务提交成功后发 PERSISTED_ACK。
- delivered：收件端 SQLite 事务完成后发 DELIVERY_ACK，服务器 send() 成功不足以证明。
- read：聊天页实际已读后发 READ_ACK，状态只前进。
- 唯一键 (sender_id, client_message_id) 去重；同 ID/同内容返回原结果，同 ID/不同内容返回冲突。
- 超时以有限指数退避与抖动重试，message_id 不变，request_id 可以变化。
- 断线后重新认证，再按持久化用户游标分页同步；客户端本地事务后推进游标，重复推送仍可确认。

单账号新登录替换旧登录，使用会话代次，旧连接的迟到任务不能清理新登录。用户资料、历史与会话访问必须验证成员权限。图片/文件不放入无界 TCP 帧，撤回是持久化事件。

认证凭据需要 TLS 或受控加密隧道；当前无认证的 loopback 诊断 TCP 不具备生产部署条件。

## 验证与版本

实际测试包含每个拆包边界、二进制内容、最大帧、多帧/尾部半帧、非法长度整数边界/版本/flags、Protobuf 畸形与深度、身份伪造、140 连续请求、最大 Envelope 后跟一帧、16 客户端独立回包、超时和关闭竞态。重复业务消息、数据库提交后断线和离线分页尚未测试。

当前固定 protoc/runtime 3.21.12（Ubuntu 开发基线）；CMake 要求精确匹配，不能混用生成器与 runtime。依赖来源见 [dependencies.md](dependencies.md)，实际命令与结果见 [progress.md](progress.md)。未来新增业务字段时保持兼容性，并重新验证两端。
