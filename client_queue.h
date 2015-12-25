#pragma once

#include <list>
#include <random>
#include "client.h"
class client_queue
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution = std::uniform_int_distribution<int>(1, 50); 

    //CRITICAL_SECTION sec;
    std::mutex sec;
    std::list<Client*> q;
public:
    void push(Client* cl);
    void remove(Client* cl);
    size_t size() const noexcept;
    Client* pop();

    //Makes pair and sets the theme for conversation
    void make_pair(Client * cl);
    client_queue(const client_queue& other) = delete;
    client_queue& operator=(const client_queue& other) = delete;
    client_queue();
    ~client_queue();
};

