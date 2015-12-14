#include "client_storage.h"


void client_storage::attach_client(Client* cl)
{
    EnterCriticalSection(&cs_clientList);

    storage.push_back(cl);

    LeaveCriticalSection(&cs_clientList);
}

void client_storage::detach_client(Client* cl)
{
    EnterCriticalSection(&cs_clientList);

    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        if (cl == *it)
        {
            storage.erase(it);
            delete cl;
            break;
        }
    }

    LeaveCriticalSection(&cs_clientList);
}

void client_storage::clear_all()
{
    EnterCriticalSection(&cs_clientList);

    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        delete *it;
    }
    storage.clear();

    LeaveCriticalSection(&cs_clientList);
}

std::vector<Client*> const& client_storage::watch_clients() const
{
    return storage;
}

unsigned int client_storage::clients_count() const
{
    return static_cast<unsigned>(storage.size());
}

client_storage::client_storage()
{
    InitializeCriticalSection(&cs_clientList);
}


client_storage::~client_storage()
{
    clear_all();
    DeleteCriticalSection(&cs_clientList);
}
