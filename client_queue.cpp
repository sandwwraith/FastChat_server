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
    while(q.size() > 0)
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

//void client_queue::remove(Client* cl)
//{
//    std::lock_guard<std::mutex> guard(sec);
//    for (auto it = q.begin(); it != q.end(); ++it)
//    {
//        if (*it == cl) 
//        {
//            q.erase(it);
//            break;
//        }
//    }
//}

//void client_queue::push(Client* cl)
//{
//    std::lock_guard<std::mutex> guard(sec);
//    q.push_back(cl); //Would be better if this operation is atomic
//}
//
//Client* client_queue::pop()
//{
//    if (q.size() < 1) return nullptr;
//    std::lock_guard<std::mutex> guard(sec);
//    Client* res;
//    try 
//    {
//        res = q.front();
//        q.pop_front();
//    } catch(...)
//    {
//        return nullptr;
//    }
//    /*this->unlock();*/
//    sec.unlock();
//    return res;
//}

std::shared_ptr<Client> client_queue::try_pair(std::shared_ptr<Client> const& client)
{
    auto pair = this->pop();
    if (pair)
    {
        client->set_companion(std::weak_ptr<Client>(pair));
        pair->set_companion(std::weak_ptr<Client>(client));

        char theme = distribution.operator()(generator);
        client->q_msg[2] = theme;
        pair->q_msg[2] = theme;
        return pair;
    }
    return std::shared_ptr<Client>();
}

std::shared_ptr<Client> client_queue::pair_or_queue(std::shared_ptr<Client> const& cl)
{
    auto p = try_pair(cl);
    if (!p)
        this->push(cl);
    return p;
}

//void client_queue::make_pair(Client* cl)
//{
//    Client* pair = this->pop();
//    pair->set_companion(cl);
//    cl->set_companion(pair);
//
//    char theme = distribution.operator()(generator);
//    cl->q_msg[2] = theme;
//    pair->q_msg[2] = theme;
//}


client_queue::client_queue()
{
    uint64_t seed = std::chrono::system_clock::now().time_since_epoch().count();
    generator = std::default_random_engine(static_cast<unsigned int>(seed));
}


client_queue::~client_queue()
{
}
