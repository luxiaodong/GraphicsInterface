
#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

class Thread
{
public:
    Thread();
    ~Thread();
    
    void queueLoop();
    void addJob(std::function<void()> function);
    void wait();
    
private:
    bool m_destroying = false;
    std::thread m_worker;
    std::queue<std::function<void()>> m_jobQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
};

class ThreadPool
{
public:
    void setThreadCount(uint32_t count);
    void wait();
    
public:
    std::vector<std::unique_ptr<Thread>> m_threads;
};

