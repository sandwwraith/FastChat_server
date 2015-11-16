#pragma once
#include "client.h"
#include <list>

class client_storage
{
    std::list<Client*> storage;
public:
    void attach_client(Client*);
    void detach_client(Client*);
    void clear_all();
    client_storage();
    ~client_storage();
};
