#include "concurrency/boundedthreadpool.h"
#include <stdexcept>
#include <utility>

namespace
{
thread_local const littlechat::server::BoundedThreadPool *currentPool = nullptr;
}
namespace littlechat
{
namespace server
{
BoundedThreadPool::BoundedThreadPool(std::size_t workers, std::size_t capacity, ErrorHandler errorHandler)
    : m_capacity(capacity), m_errorHandler(std::move(errorHandler)), m_accepting(true), m_stopping(false),
      m_drain(true), m_active(0), m_failed(0)
{
    if (workers == 0 || capacity == 0)
        throw std::invalid_argument("worker count and queue capacity must be positive");
    try
    {
        m_workers.reserve(workers);
        for (std::size_t i = 0; i < workers; ++i)
            m_workers.emplace_back([this] { worker(); });
    }
    catch (...)
    {
        requestStop(StopMode::CancelPending);
        for (std::thread &thread : m_workers)
            if (thread.joinable())
                thread.join();
        throw;
    }
}
BoundedThreadPool::~BoundedThreadPool()
{
    shutdown(StopMode::Drain);
}
SubmitResult BoundedThreadPool::trySubmit(Task task)
{
    if (!task)
        return SubmitResult::Empty;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_accepting)
            return SubmitResult::Stopped;
        if (m_tasks.size() >= m_capacity)
            return SubmitResult::Full;
        m_tasks.push(std::move(task));
    }
    m_ready.notify_one();
    return SubmitResult::Accepted;
}
void BoundedThreadPool::requestStop(StopMode mode)
{
    std::queue<Task> discarded;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
            return;
        m_accepting = false;
        m_stopping = true;
        m_drain = mode == StopMode::Drain;
        if (!m_drain)
            m_tasks.swap(discarded);
    }
    // Captured resources in cancelled tasks are destroyed outside the queue mutex.
    m_ready.notify_all();
    m_idle.notify_all();
}
void BoundedThreadPool::shutdown(StopMode mode)
{
    if (currentPool == this)
        throw std::logic_error("a worker cannot join its own pool; use requestStop");
    std::lock_guard<std::mutex> joinLock(m_joinMutex);
    requestStop(mode);
    for (std::thread &thread : m_workers)
        if (thread.joinable())
            thread.join();
}
void BoundedThreadPool::waitIdle()
{
    if (currentPool == this)
        throw std::logic_error("a worker cannot wait for its own task to become idle");
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_tasks.empty() && m_active == 0; });
}
std::size_t BoundedThreadPool::pending() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.size();
}
void BoundedThreadPool::worker()
{
    currentPool = this;
    for (;;)
    {
        Task task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
            if (m_stopping && (!m_drain || m_tasks.empty()))
                break;
            task = std::move(m_tasks.front());
            m_tasks.pop();
            ++m_active;
        }
        try
        {
            task();
        }
        catch (...)
        {
            ++m_failed;
            if (m_errorHandler)
            {
                try
                {
                    m_errorHandler(std::current_exception());
                }
                catch (...)
                {
                    // A failing error observer must not terminate a worker.
                }
            }
        }
        task = Task();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            --m_active;
            if (m_tasks.empty() && m_active == 0)
                m_idle.notify_all();
        }
    }
    currentPool = nullptr;
}
} // namespace server
} // namespace littlechat
