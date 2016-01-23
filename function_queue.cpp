#include "stdafx.h"
#include "function_queue.h"

void function_queue::safe_push(_fq_event const& e)
{
    std::unique_lock<std::mutex> guard{ mut };
    event_q.push(e);
    cv.notify_one();
}

_fq_event function_queue::safe_pop()
{
    auto even = event_q.top();
    event_q.pop();
    return even;
}

void function_queue::worker(function_queue* me)
{
    while (!me->cancelled)
    {
        std::unique_lock<std::mutex> lck{ me->mut };
        while (me->event_q.empty()) {
            me->cv.wait(lck);
            if (me->cancelled) return;
        }

        auto t = me->event_q.top().at;
        while (std::chrono::system_clock::now() < t && !me->cancelled) me->cv.wait_until(lck, t);
        if (me->cancelled) return;
        auto e = me->safe_pop();
        e.runnable();
    }
}

void function_queue::enqueue(std::function<void()> f, int seconds_delay)
{
    safe_push(_fq_event(std::move(f),seconds_delay));
}

function_queue::function_queue()
{
    runner = std::thread(function_queue::worker, this);
}

function_queue::~function_queue()
{
    cancelled = true;
    cv.notify_all();
    if (runner.joinable()) runner.join();
}
