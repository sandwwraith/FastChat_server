#include "stdafx.h"
#include "wrappers.h"

#include "server.h"


ThreadPool::ThreadPool(server* host) : host(host)
{
    int g_workers_count =
#ifndef _DEBUG
        g_worker_threads_per_processor * get_proc_count();
#else
        2;
#endif
    threads.reserve(g_workers_count);
    std::cout << "Threads count: " << g_workers_count << std::endl;
    for (auto i = 0; i < g_workers_count;i++)
    {
        HANDLE a = CreateThread(nullptr, 0, server::WorkerThread, host, 0, nullptr);
        if (a == INVALID_HANDLE_VALUE)
        {
            closethreads();
            throw std::runtime_error("Cannot create thread: " + GetLastError());
        }
        threads.push_back(a);
    }
}

void ThreadPool::closethreads()
{
    for (size_t i = 0;i < threads.size(); i++)
    {
        //Signal for threads - if they get NULL context, they shutdown.
        host->IOCP.post(nullptr, nullptr, 0);
    }
    while (!threads.empty())
    {
        WaitForSingleObject(threads.back(), INFINITE);
        threads.pop_back();
    }
}