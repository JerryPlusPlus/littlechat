set(LITTLECHAT_MUDUO_SOURCE_DIR "${LITTLECHAT_DEPENDENCY_ROOT}/muduo-src" CACHE PATH "Pinned muduo v2.0.2 checkout")
littlechat_verify_checkout("${LITTLECHAT_MUDUO_SOURCE_DIR}" b55b0ac6a10d4ded1301c3172e779ec46ada2ab3
    "${CMAKE_CURRENT_LIST_DIR}/../patches/muduo-stop-accepting.patch")
find_package(Threads REQUIRED)
find_package(Boost 1.83 CONFIG REQUIRED)

# Build upstream sources plus the reviewed lifecycle patch, without obsolete global flags/-march=native.
set(muduo_base_names AsyncLogging Condition CountDownLatch CurrentThread Date Exception FileUtil
    LogFile Logging LogStream ProcessInfo Timestamp Thread ThreadPool TimeZone)
set(muduo_net_names Acceptor Buffer Channel Connector EventLoop EventLoopThread EventLoopThreadPool
    InetAddress Poller poller/DefaultPoller poller/EPollPoller poller/PollPoller Socket SocketsOps
    TcpClient TcpConnection TcpServer Timer TimerQueue)
set(muduo_base_sources "")
set(muduo_net_sources "")
foreach(name IN LISTS muduo_base_names)
    list(APPEND muduo_base_sources "${LITTLECHAT_MUDUO_SOURCE_DIR}/muduo/base/${name}.cc")
endforeach()
foreach(name IN LISTS muduo_net_names)
    list(APPEND muduo_net_sources "${LITTLECHAT_MUDUO_SOURCE_DIR}/muduo/net/${name}.cc")
endforeach()
add_library(littlechat_muduo_base STATIC ${muduo_base_sources})
target_include_directories(littlechat_muduo_base SYSTEM PUBLIC "${LITTLECHAT_MUDUO_SOURCE_DIR}" ${Boost_INCLUDE_DIRS})
target_link_libraries(littlechat_muduo_base PUBLIC Threads::Threads rt littlechat_build_options)
add_library(littlechat_muduo_net STATIC ${muduo_net_sources})
target_link_libraries(littlechat_muduo_net PUBLIC littlechat_muduo_base)
