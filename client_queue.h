#pragma once

#include <list>
#include <random>
#include "client2.h"
class client_queue
{
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution{ 1, 49 };

    std::mutex sec;
    std::list<std::weak_ptr<Client>> q;
public:
    void push(std::shared_ptr<Client>const&);
    size_t size() const noexcept;
    std::shared_ptr<Client>pop();

    //Makes pair and sets the theme for conversation
    std::shared_ptr<Client> try_pair(std::shared_ptr<Client> const&);

    std::shared_ptr<Client> pair_or_queue(std::shared_ptr < Client>const&);

    client_queue(const client_queue& other) = delete;
    client_queue& operator=(const client_queue& other) = delete;
    client_queue();
    ~client_queue();

    char generate_random();
};

