#include "client_queue.h"

void client_queue::unlock()
{
    LeaveCriticalSection(&sec);
}

size_t client_queue::size() const
{
    return q.size();
}

void client_queue::push(Client* cl)
{
    this->lock();
    q.push(cl); //Would be better if this operation is atomic
    this->unlock();
}

Client* client_queue::pop()
{
    if (q.size() < 1) return nullptr;
    this->lock();
    Client* res = q.front();
    q.pop();
    this->unlock();
    return res;
}

void client_queue::make_pair(Client* cl)
{
    Client* pair = this->pop();
    pair->set_companion(cl);
    cl->set_companion(pair);

    char theme = distribution.operator()(generator);
    cl->get_buffer_data()[2] = theme;
    pair->get_buffer_data()[2] = theme;
}

void client_queue::lock()
{
    EnterCriticalSection(&sec);
}

client_queue::client_queue()
{
    InitializeCriticalSection(&sec);
}


client_queue::~client_queue()
{
    DeleteCriticalSection(&sec);
}
