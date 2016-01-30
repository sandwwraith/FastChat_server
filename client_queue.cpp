#include "stdafx.h"
#include "client_queue.h"

#include <random>
#include <chrono>


size_t client_queue::size() const noexcept
{
    return q.size();
}

std::shared_ptr<Client> client_queue::pop()
{
    std::lock_guard<std::mutex> guard{ sec };
    while (q.size() > 0)
    {
        std::shared_ptr<Client> ptr = q.front().lock();
        q.pop_front();
        if (ptr)
        {
            return ptr;
        }
        continue;
    }
    return std::shared_ptr<Client>();
}

void client_queue::push(std::shared_ptr<Client> const& ptr)
{
    std::lock_guard<std::mutex> guard{ sec };
    q.push_back(std::weak_ptr<Client>(ptr));
}

std::shared_ptr<Client> client_queue::pair_or_queue(std::shared_ptr<Client> const& cl)
{
    auto p = pop();
    if (!p)
        this->push(cl);
    return p;
}

client_queue::client_queue()
#ifndef _DEBUG
    : generator(std::default_random_engine(static_cast<unsigned int>(current_time())))
#endif
{}


client_queue::~client_queue() {}

char client_queue::generate_random()
{
    return distribution(generator);
}