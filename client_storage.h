#pragma once
#include "client.h"
#include <list>

class client_storage
{
    std::list<client_context*> storage;

    //Mutex for working with storage
    std::mutex cs_clientList;
public:
    void attach_client(client_context*);
    client_storage(const client_storage& other) = delete;
    client_storage& operator=(const client_storage& other) = delete;
    void detach_client(client_context*);
    void clear_all();

    std::list<client_context*> const& watch_clients() const;
    unsigned int clients_count() const noexcept;
    client_storage();
    ~client_storage();
};
