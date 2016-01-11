#include "stdafx.h"
#include "client_storage.h"


void client_storage::attach_client(client_context* cl)
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    storage.push_back(cl);

}

void client_storage::detach_client(client_context* cl) noexcept
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

unsigned int client_storage::clients_count() noexcept
{
    std::lock_guard<std::mutex> guard { cs_clientList };
    return static_cast<unsigned>(storage.size());
}

client_storage::client_storage()
{
}


client_storage::~client_storage()
{
    clear_all();
}
