#pragma once
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace littlechat
{
namespace server
{
enum class SubmitResult
{
    Accepted,
    Full,
    Stopped,
    Empty
};
enum class StopMode
{
    Drain,
    CancelPending
};

// The owner must outlive all tasks and destroy/join the pool from a non-worker thread.
// Rejected submissions are never executed. trySubmit never waits for queue capacity.
class BoundedThreadPool
{
  public:
    typedef std::function<void()> Task;
    // Observers may be called concurrently and must provide their own synchronization.
    typedef std::function<void(std::exception_ptr)> ErrorHandler;
    BoundedThreadPool(std::size_t workers, std::size_t capacity, ErrorHandler errorHandler = ErrorHandler());
    ~BoundedThreadPool();
    BoundedThreadPool(const BoundedThreadPool &) = delete;
    BoundedThreadPool &operator=(const BoundedThreadPool &) = delete;
    SubmitResult trySubmit(Task task);
    void requestStop(StopMode mode = StopMode::Drain);
    void shutdown(StopMode mode = StopMode::Drain);
    void waitIdle();
    std::size_t pending() const;
    std::size_t failedTasks() const
    {
        return m_failed.load();
    }

  private:
    void worker();
    const std::size_t m_capacity;
    ErrorHandler m_errorHandler;
    mutable std::mutex m_mutex;
    std::mutex m_joinMutex;
    std::condition_variable m_ready;
    std::condition_variable m_idle;
    std::queue<Task> m_tasks;
    bool m_accepting;
    bool m_stopping;
    bool m_drain;
    std::size_t m_active;
    std::atomic<std::size_t> m_failed;
    std::vector<std::thread> m_workers;
};
} // namespace server
} // namespace littlechat
