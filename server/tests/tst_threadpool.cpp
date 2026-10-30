#include "concurrency/boundedthreadpool.h"
#include <atomic>
#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>

using namespace littlechat::server;
TEST(ThreadPool, RejectsInvalidSizesAndEmptyTasks)
{
    EXPECT_THROW(BoundedThreadPool(0, 1), std::invalid_argument);
    EXPECT_THROW(BoundedThreadPool(1, 0), std::invalid_argument);
    BoundedThreadPool pool(1, 1);
    EXPECT_EQ(pool.trySubmit(BoundedThreadPool::Task()), SubmitResult::Empty);
}
TEST(ThreadPool, RejectsFullQueueWithoutBlockingAndDrainsAcceptedTasks)
{
    std::promise<void> entered, release;
    std::future<void> started = entered.get_future();
    const std::shared_future<void> proceed = release.get_future().share();
    BoundedThreadPool pool(1, 1);
    std::atomic<int> finished(0);
    ASSERT_EQ(pool.trySubmit(
                  [&entered, proceed]
                  {
                      entered.set_value();
                      proceed.wait_for(std::chrono::seconds(2));
                  }),
              SubmitResult::Accepted);
    ASSERT_EQ(started.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    ASSERT_EQ(pool.trySubmit([&finished] { ++finished; }), SubmitResult::Accepted);
    EXPECT_EQ(pool.trySubmit([] {}), SubmitResult::Full);
    EXPECT_EQ(pool.pending(), 1U);
    release.set_value();
    pool.shutdown();
    EXPECT_EQ(finished.load(), 1);
    EXPECT_EQ(pool.trySubmit([] {}), SubmitResult::Stopped);
}
TEST(ThreadPool, CancelPendingPreservesRunningTaskAndDiscardsQueuedTask)
{
    std::promise<void> entered, release;
    std::future<void> started = entered.get_future();
    const std::shared_future<void> proceed = release.get_future().share();
    BoundedThreadPool pool(1, 2);
    std::atomic<int> runningFinished(0), queuedFinished(0);
    ASSERT_EQ(pool.trySubmit(
                  [&entered, &runningFinished, proceed]
                  {
                      entered.set_value();
                      proceed.wait_for(std::chrono::seconds(2));
                      ++runningFinished;
                  }),
              SubmitResult::Accepted);
    ASSERT_EQ(started.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    ASSERT_EQ(pool.trySubmit([&queuedFinished] { ++queuedFinished; }), SubmitResult::Accepted);
    pool.requestStop(StopMode::CancelPending);
    release.set_value();
    pool.shutdown();
    EXPECT_EQ(runningFinished.load(), 1);
    EXPECT_EQ(queuedFinished.load(), 0);
}
TEST(ThreadPool, ExceptionsAndFailingObserverDoNotKillWorker)
{
    std::atomic<int> observed(0), finished(0);
    BoundedThreadPool pool(1, 4,
                           [&observed](std::exception_ptr failure)
                           {
                               EXPECT_NE(failure, nullptr);
                               ++observed;
                               throw std::runtime_error("observer failure");
                           });
    ASSERT_EQ(pool.trySubmit([] { throw std::runtime_error("task failure"); }), SubmitResult::Accepted);
    ASSERT_EQ(pool.trySubmit([&finished] { ++finished; }), SubmitResult::Accepted);
    pool.waitIdle();
    EXPECT_EQ(observed.load(), 1);
    EXPECT_EQ(finished.load(), 1);
    EXPECT_EQ(pool.failedTasks(), 1U);
}
TEST(ThreadPool, ConcurrentProducersLoseNoAcceptedTasks)
{
    BoundedThreadPool pool(2, 8);
    std::atomic<unsigned> accepted(0), finished(0);
    std::vector<std::thread> producers;
    for (unsigned i = 0; i < 4; ++i)
    {
        producers.emplace_back(
            [&pool, &accepted, &finished]
            {
                for (unsigned j = 0; j < 100; ++j)
                    if (pool.trySubmit([&finished] { ++finished; }) == SubmitResult::Accepted)
                        ++accepted;
            });
    }
    for (std::thread &producer : producers)
        producer.join();
    pool.shutdown();
    EXPECT_GT(accepted.load(), 0U);
    EXPECT_EQ(finished.load(), accepted.load());
}
TEST(ThreadPool, OwnerCanJoinAfterWorkerRequestsStop)
{
    BoundedThreadPool pool(1, 1);
    ASSERT_EQ(pool.trySubmit([&pool] { pool.requestStop(); }), SubmitResult::Accepted);
    pool.waitIdle();
    EXPECT_EQ(pool.trySubmit([] {}), SubmitResult::Stopped);
    pool.shutdown();
}
TEST(ThreadPool, WorkerCannotJoinOrWaitOnItself)
{
    BoundedThreadPool pool(1, 2);
    std::atomic<int> rejected(0);
    ASSERT_EQ(pool.trySubmit(
                  [&pool, &rejected]
                  {
                      try
                      {
                          pool.shutdown();
                      }
                      catch (const std::logic_error &)
                      {
                          ++rejected;
                      }
                      try
                      {
                          pool.waitIdle();
                      }
                      catch (const std::logic_error &)
                      {
                          ++rejected;
                      }
                  }),
              SubmitResult::Accepted);
    pool.waitIdle();
    EXPECT_EQ(rejected.load(), 2);
}
TEST(ThreadPool, ReleasesTaskOwnershipBeforeBecomingIdle)
{
    BoundedThreadPool pool(1, 1);
    std::shared_ptr<int> resource(new int(7));
    const std::weak_ptr<int> observer(resource);
    ASSERT_EQ(pool.trySubmit([resource] { EXPECT_EQ(*resource, 7); }), SubmitResult::Accepted);
    resource.reset();
    pool.waitIdle();
    EXPECT_TRUE(observer.expired());
}
