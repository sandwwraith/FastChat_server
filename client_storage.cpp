#include "client_storage.h"


void client_storage::attach_client(Client* cl)
{
    storage.push_back(cl);
}

void client_storage::detach_client(Client* cl)
{
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
    for (auto it = storage.begin(), end = storage.end(); it != end; ++it)
    {
        delete *it;
    }
    storage.clear();
}

client_storage::client_storage()
{}


client_storage::~client_storage()
{
    clear_all();
}
