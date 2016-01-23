#pragma once
#include "client2.h"
#include <list>

class client_storage
{
    std::list<std::unique_ptr<client_context> > storage;

    //Mutex for working with storage
    std::mutex cs_clientList;
public:
    void attach_client(std::unique_ptr<client_context>);
    client_storage(const client_storage& other) = delete;
    client_storage& operator=(const client_storage& other) = delete;
    void detach_client(client_context*) noexcept;
    void clear_all();

    unsigned int clients_count() noexcept;
    client_storage() {};
};
