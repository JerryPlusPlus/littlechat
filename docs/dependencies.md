# 依赖与环境记录

更新时间：2026-10-22。Windows 客户端与 Linux 网络骨架已实际构建；数据库与缓存依赖尚未验收。

## 已使用版本

| 依赖/环境 | 锁定或实际版本 | 说明 |
| --- | --- | --- |
| Qt | 本机 6.8.3 / MinGW GCC 13.1.0；MSVC 2022 套件 | C++17；Widgets/Svg/Qt Test；Network/Sql 驱动存在但未接入 |
| Ubuntu | WSL VSRemote，24.04.3 LTS | 已通过审批在沙箱外运行，未安装/重配 WSL |
| GCC / CMake | 13.3.0 / 4.1.5，Ninja | Debug、Release、ASan/UBSan 已构建，语言扩展关闭 |
| muduo | v2.0.2，b55b0ac6a10d4ded1301c3172e779ec46ada2ab3 | BSD-3-Clause；显式源码构建，添加下述唯一补丁 |
| GoogleTest | v1.15.2，b514bdc898e2951020cbdca1304b75f5950d1f59 | BSD-3-Clause；项目按 C++17 实测编译，保留 Qt Test |
| Protobuf | protoc/runtime 3.21.12 | Ubuntu libprotobuf-dev / protobuf-compiler 3.21.12-8.2ubuntu0.4；BSD-3-Clause |
| Boost | config 1.83.0；libboost-dev 1.83.0.1ubuntu2 | Boost Software License；用于 muduo 的头文件 |
| pthread / rt | Ubuntu 系统库 | Linux 线程与 muduo 依赖 |

源码来自 [muduo 官方 tag](https://github.com/chenshuo/muduo/tree/v2.0.2) 和 [GoogleTest 官方 tag](https://github.com/google/googletest/tree/v1.15.2)。脚本 scripts/fetch-linux-deps.sh 显式获取到被 Git 忽略的 build/_deps，验证完整 commit，不覆盖不匹配目录，不 reset 用户修改。CMake 再次校验，不在配置中下载。

Protobuf 采用当前已装的 Ubuntu 补丁包，以便先完成可验证的 C++17 开发骨架。它比先前考虑的 6.33.x 新分支更旧，不作为生产版本建议。本次没有安装/降级系统包。升级前需核验维护状态、C++17 兼容性、生成代码/runtime 配套及两端协议回归；不得自动升级整个项目到 C++20。精确版本由 LITTLECHAT_PROTOBUF_VERSION 控制，当前默认为 3.21.12，CMake 同时检查 protoc --version。

## muduo 受控补丁

路径：server/patches/muduo-stop-accepting.patch。

SHA256：`38fd68f32f3843170221186c092d3ee31927f551680ea4618c25806794e685ee`。

仅在 TcpServer.h/.cc 增加 stopAccepting()，在 owner EventLoop 断言后释放 acceptor。上游公开接口不能提前停止监听；本项目需要在停服期间拒绝新连接并正确清理已派发任务。调用必须在 pending-functor 阶段，避开活跃 Channel 列表。

获取脚本支持首次应用/重复验证，CMake 检查 HEAD 与精确 diff，拒绝其他修改。没有覆盖源码许可证。自有 CMake 包装避开上游旧 CMake 最低版本及全局 -march=native/-Werror，使用 C++17 编译实际 muduo 源码；没有另行实现 Reactor。

## 尚未接入

| 依赖 | 下一阶段动作 |
| --- | --- |
| MySQL 8 | VSRemote 已有 mysql/mysqld 8.0.46-0ubuntu0.24.04.4 与本地 socket；未连接数据。缺少 mysql.h / libmysqlclient.so 开发链接，P3 准备隔离测试实例与开发依赖 |
| Argon2id | 缺少 argon2.h / libargon2.so；P3 选定维护中的兼容库，核验随机盐、参数和向量 |
| Redis | P5 固定版本/镜像摘要，验证 TTL/会话代次与退化路径 |
| Docker Compose | Windows CLI 已有 v5.3.1；Linux 引擎未连接，不自动启动或改服务 |
| Windows Protobuf | P4 接入 Qt 网络时准备同版本生成器/runtime，不能复用 Linux 二进制 |

所有版本升级都记录来源、许可证、C++ 标准、命令与实际结果。镜像不能只写 latest。当前 ASan/UBSan 覆盖自有协议、服务器、线程池及源码构建的 muduo；系统 Protobuf/Boost 及 GoogleTest 库未单独启用 Sanitizer。未运行 TSan，不将 Sanitizer 通过当成无竞态证明。
