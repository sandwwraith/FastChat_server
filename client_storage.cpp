#include "stdafx.h"
#include "client_storage.h"


void client_storage::attach_client(Client* cl)
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    storage.push_back(cl);

}

void client_storage::detach_client(Client* cl)
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        if (cl == *it)
        {
            storage.erase(it);
            delete cl;
            break;
        }
    }
}

void client_storage::clear_all()
{
    std::lock_guard<std::mutex> guard{ cs_clientList };

    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        delete *it;
    }
    storage.clear();

}

std::list<Client*> const& client_storage::watch_clients() const
{
    return storage;
}

unsigned int client_storage::clients_count() const
{
    return static_cast<unsigned>(storage.size());
}

client_storage::client_storage()
{
}


client_storage::~client_storage()
{
    clear_all();
}
