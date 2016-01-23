#include "stdafx.h"
#include "client_storage.h"

void client_storage::attach_client(std::unique_ptr<client_context> cl)
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    storage.push_back(std::move(cl));

}

void client_storage::detach_client(client_context* cl) noexcept
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        if (cl == it->get())
        {
            storage.erase(it);
            break;
        }
    }
}

void client_storage::clear_all()
{
    std::lock_guard<std::mutex> guard{ cs_clientList };
    storage.clear();
}

unsigned int client_storage::clients_count() noexcept
{
    std::lock_guard<std::mutex> guard { cs_clientList };
    return static_cast<unsigned>(storage.size());
}