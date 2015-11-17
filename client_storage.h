#pragma once
#include "client.h"
#include <vector>

class client_storage
{
    std::vector<Client*> storage;

    //Mutex for working with storage
    CRITICAL_SECTION cs_clientList;
public:
    void attach_client(Client*);
    void detach_client(Client*);
    void clear_all();

    std::vector<Client*> const& watch_clients() const;
    unsigned int clients_count() const;
    client_storage();
    ~client_storage();
};
