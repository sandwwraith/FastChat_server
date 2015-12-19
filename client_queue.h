#pragma once

#include <list>
#include <random>
#include <chrono>
#include "client.h"
class client_queue
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution = std::uniform_int_distribution<int>(1, 50); 

    CRITICAL_SECTION sec;
    std::list<Client*> q;
    void lock();
    void unlock();
public:
    void push(Client* cl);
    void remove(Client* cl);
    size_t size() const noexcept;
    Client* pop();

    //Makes pair and sets the theme for conversation
    void make_pair(Client * cl);
    client_queue();
    ~client_queue();
};

