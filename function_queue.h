#pragma once

#include <queue>
#include <chrono>
struct _fq_event
{
    std::function<void()> runnable;
    std::chrono::system_clock::time_point at;

    _fq_event(const std::function<void()>&& runnable, int delay)
        : runnable(runnable),
          at(std::chrono::system_clock::now() + std::chrono::duration<int>(delay))
    {
    }
};
struct _fq_event_comparator
{
    bool operator() (_fq_event const&l, _fq_event const& r) {
        return l.at > r.at; //Because default heap is maximum-based
    }
};
class function_queue
{
    std::priority_queue<_fq_event, std::vector<_fq_event>, _fq_event_comparator> event_q;
    std::mutex mut;
    std::condition_variable cv;
    std::thread runner;

    void safe_push(_fq_event const&);
    _fq_event safe_pop();
    static void worker(function_queue* me);
public:
    void enqueue(std::function<void()> f, int delay);
    function_queue();
    ~function_queue();
    function_queue(const function_queue& other) = delete;
    function_queue& operator=(const function_queue& other) = delete;
};
