#pragma once
#include "client.h"
#include <list>

class client_storage
{
    std::list<Client*> storage;

    //Mutex for working with storage
    //CRITICAL_SECTION cs_clientList;
    std::mutex cs_clientList;
public:
    void attach_client(Client*);
    client_storage(const client_storage& other) = delete;
    client_storage& operator=(const client_storage& other) = delete;
    void detach_client(Client*);
    void clear_all();

    std::list<Client*> const& watch_clients() const;
    unsigned int clients_count() const;
    client_storage();
    ~client_storage();
};
