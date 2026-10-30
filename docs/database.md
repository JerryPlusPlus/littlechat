# 数据库与消息事务设计

状态：P1 逻辑设计。尚未执行 MySQL 迁移、Redis 命令或 SQLite 迁移，没有数据库集成测试结果。

## MySQL 8（可靠数据来源）

计划使用 InnoDB、utf8mb4、UTC 时间和参数化 SQL。ID/时间传输时避免 JSON 浮点精度问题。按查询路径建立索引，不为每个字段创建索引。

| 表 | 关键列与约束 | 查询索引 |
| --- | --- | --- |
| users | user_id PK；规范化账号 UNIQUE；password_hash 含算法/盐/参数；display_name；created_at | 账号唯一键 |
| friendships | owner_id、friend_id 复合 PK；状态和备注；外键 | friend_id |
| conversations | conversation_id PK；类型；单聊规范化用户对 UNIQUE | 单聊用户对 |
| conversation_members | conversation_id、user_id 复合 PK；角色；加入时间 | user_id、conversation_id |
| messages | internal_id PK；sender_id；client_message_id BINARY(16)；conversation_id；类型/正文；服务端时间 | UNIQUE(sender_id,client_message_id)；(conversation_id,internal_id) |
| message_recipients | message_id、recipient_id 复合 PK；delivered_at、read_at | (recipient_id,delivery_state,message_id) |
| sync_counters | user_id PK；next_sequence；由事务锁定分配序列 | 主键 |
| sync_events | user_id、sequence 复合 PK；事件类型；消息引用；事件版本 | 按主键游标分页；事件唯一约束 |
| sessions | session_id PK；user_id；代次；令牌摘要；过期/撤销时间 | user_id、expires_at |
| schema_migrations | version PK；应用时间和校验信息 | 主键 |

`messages` 只存一份消息；`message_recipients` 保存收件人状态。`sync_events` 同时承担持久化待同步事件，第一版不引入额外消息中间件。回执和撤回也可作为事件送给离线发送者。

## SEND_TEXT 事务边界

1. Worker 获取数据库连接租约，开始事务，验证用户/会话成员权限。
2. 插入消息，利用唯一键处理重试。遇重复键时读取原记录并核对正文、接收者等不可变字段；不一致返回冲突。
3. 写收件人状态；按固定 user_id 顺序锁定涉及的 `sync_counters` 行，分配每用户事件序列，插入同步事件。
4. 提交成功后，将结果投递回所属 IO EventLoop，返回持久化 ACK 并尝试在线推送。
5. 失败回滚，不发送持久化 ACK；错误映射为可重试/不可重试结果。

禁止直接用全局 AUTO_INCREMENT 最大值作为同步高水位：序号分配顺序不保证事务提交顺序，可能跳过较小但迟提交的记录。用户事件计数行的锁持有到事务提交，确保该用户序列按提交顺序可见；客户端只推进已完成本地事务的最后一条事件序号。

回执处理锁定收件人状态，仅在状态前进时写相应同步事件；重复 ACK 不制造重复事件。数据库唯一约束作为最后一道去重保障，不能只靠进程内集合或 Redis。

## 线程、连接池与故障

- MySQL 连接只由取得租约的业务 Worker 使用，使用后通过 RAII 返回池；不跨线程共享活动连接。
- 池容量、等待队列、连接/查询/事务超时有界。IO EventLoop 不等待数据库租约。
- 丢失连接、死锁、提交结果不明时，用消息 ID 查询/重试恢复；不能凭连接错误就断言事务未提交。
- 查询历史使用稳定的 keyset 游标，避免大 OFFSET；所有用户查询验证成员权限。
- 对账号、用户输入和文件元数据统一使用绑定参数，日志隐藏凭据与正文。

## Redis（P5）

仅用于在线状态 TTL、会话代次提示和热点资料缓存。在线连接本体仍由本进程 SessionRegistry 管理；缓存缺失/故障不能导致已提交消息丢失。更新/清理在线键时核对会话代次，避免旧连接删除新登录状态。

不将 Redis 在线标记或 publish 成功作为消息已投递证明。第一版不实现跨节点通知；待单机可靠性测试稳定后再评估。

## 客户端 SQLite（P4）

计划表：users、conversations、messages、pending_outbox、sync_state、schema_migrations。消息 ID 唯一，正文与发送状态在事务中更新；输入消息落库和同步游标更新为同一事务。

数据库连接在专用存储线程创建和使用，通过 queued signal/slot 或任务队列与 UI 协作，不在 UI 线程执行耗时查询。保持现有 JSON 记录，在具备导入测试和备份策略后才迁移。Qt 6.8 的连接线程约束见[官方文档](https://doc.qt.io/qt-6.8/qsqldatabase.html)。
