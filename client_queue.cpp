#include "stdafx.h"
#include "client_queue.h"

#include <random>
#include <chrono>

void client_queue::unlock()
{
    LeaveCriticalSection(&sec);
}

size_t client_queue::size() const noexcept
{
    return q.size();
}

void client_queue::remove(Client* cl)
{
    for (auto it = q.begin(); it != q.end(); ++it)
    {
        if (*it == cl) 
        {
            q.erase(it);
            break;
        }
    }
}

void client_queue::push(Client* cl)
{
    this->lock();
    q.push_back(cl); //Would be better if this operation is atomic
    this->unlock();
}

Client* client_queue::pop()
{
    if (q.size() < 1) return nullptr;
    this->lock();
    Client* res = q.front();
    q.pop_front();
    this->unlock();
    return res;
}

void client_queue::make_pair(Client* cl)
{
    Client* pair = this->pop();
    pair->set_companion(cl);
    cl->set_companion(pair);

    char theme = distribution.operator()(generator);
    cl->q_msg[2] = theme;
    pair->q_msg[2] = theme;
}

void client_queue::lock()
{
    EnterCriticalSection(&sec);
}

client_queue::client_queue()
{
    InitializeCriticalSection(&sec);
    uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
    generator = std::default_random_engine(static_cast<unsigned int>(seed));
}


client_queue::~client_queue()
{
    DeleteCriticalSection(&sec);
}
