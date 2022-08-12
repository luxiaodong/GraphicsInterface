
#include "thread.h"

Thread::Thread()
{
    m_worker = std::thread(&Thread::queueLoop, this);
}

Thread::~Thread()
{
    if (m_worker.joinable())
    {
        wait();
        m_queueMutex.lock();
        m_destroying = true;
        m_condition.notify_one();
        m_queueMutex.unlock();
        m_worker.join();
    }
}

void Thread::queueLoop()
{
    while (true)
    {
        std::function<void()> job;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this] { return !m_jobQueue.empty() || m_destroying; });
            if (m_destroying)
            {
                break;
            }
            job = m_jobQueue.front();
        }

        job();

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_jobQueue.pop();
            m_condition.notify_one();
        }
    }
}

// Add a new job to the thread's queue
void Thread::addJob(std::function<void()> function)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_jobQueue.push(std::move(function));
    m_condition.notify_one();
}

// Wait until all work items have been finished
void Thread::wait()
{
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_condition.wait(lock, [this]() { return m_jobQueue.empty(); });
}

// Sets the number of threads to be allocated in this pool
void ThreadPool::setThreadCount(uint32_t count)
{
    m_threads.clear();
    for (auto i = 0; i < count; i++)
    {
        m_threads.push_back(std::make_unique<Thread>());
    }
}

// Wait until all threads have finished their work items
void ThreadPool::wait()
{
    for (auto &thread : m_threads)
    {
        thread->wait();
    }
}

