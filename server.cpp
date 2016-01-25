#include "stdafx.h"
#include "server.h"

DWORD server::WorkerThread(LPVOID param)
{
    server* me = static_cast<server*>(param);
    client_context* context;
    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED_EX* overlapped_ex;
    DWORD dwBytesTransfered = 0;
    while (true)
    {
        bool queued_result = me->IOCP.Dequeue(context, dwBytesTransfered, overlapped);

        if (!queued_result)
        {
            auto err_code = GetLastError();
             //121 = ERROR_SEM_TIMEOUT; 64 = NET_NAME_INVALID, need to delete client
            if (err_code == ERROR_SEM_TIMEOUT || err_code == ERROR_NETNAME_DELETED)
            {
                me->drop_client(context);
            }
            else if (err_code == ERROR_CONNECTION_ABORTED || err_code == ERROR_OPERATION_ABORTED) //Closed socket
            {
                delete overlapped;
            }
            else std::cout << "Error dequeing: " << err_code << std::endl;
            continue;
        }

        if (context == nullptr) //Signal to shutdown
        {
            return 0;
        }

        overlapped_ex = static_cast<OVERLAPPED_EX*>(overlapped);
        if (overlapped_ex->op_code == operation_code::DELETED)
        {
            delete overlapped_ex;
            continue;
        }

        if (overlapped_ex->op_code == operation_code::KEEP_ALIVE)
        {
            if (!context->isAlive())
            {
                std::cout << context->ptr->id << " idleness time exceeded" << std::endl;
                me->drop_client(context);
                delete overlapped_ex;
            }
            else
            {
                //post another kill delay
                me->g_func_queue.enqueue(context->get_upd_f(), MAX_IDLENESS_TIME);
            }
            continue;
        }

        if (overlapped_ex->op_code == operation_code::ACCEPT)
        {
            me->finish_accept();
            continue;
        }
        
        if (dwBytesTransfered == 0) //Client dropped connection
        {
            std::cout << context->ptr->id << " Zero bytes transfered, disconnecting" << std::endl;
            me->drop_client(context);
            continue;
        }
        context->updateTimer();
        context->on_overlapped_io_finished(dwBytesTransfered, overlapped_ex);
    }
}

void server::finish_accept() noexcept
{
    try {
        auto accepted = this->lastAccepted->ptr;
        if (!accepted) throw std::runtime_error("Context doesn't contain a client");
        std::cout << "Accept finishing, id:" << accepted->id << std::endl;

        int res = setsockopt(accepted->get_socket(), SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
            (char*)&(this->listenSock), sizeof(this->listenSock));
        if (res == SOCKET_ERROR)
        {
            std::cout << "Set sock opt failed: " << WSAGetLastError() << std::endl;
        }

        client_context* curr_val = this->lastAccepted.get();        
        try
        {
            this->g_client_storage.attach_client(std::move(this->lastAccepted));
            accepted->send_greetings(g_client_storage.clients_count());
            g_func_queue.enqueue(curr_val->get_upd_f(), MAX_IDLENESS_TIME);
        } 
        catch (std::exception const& ex)
        {
            std::cout << "Can't accept this client: " << ex.what() << std::endl;
            this->drop_client(curr_val);
        }
        this->accept();
    }
    catch (std::exception const& ex)
    {
        std::cout << "Fatal ACCEPT error: " << ex.what() << std::endl;
        //WAT TO DO ???
    }
}


void server::accept()
{
    GUID GuidAcceptEx = WSAID_ACCEPTEX;
    LPFN_ACCEPTEX lpfnAcceptEx = nullptr;
    DWORD dwBytes;
    //The most amazing function i've ever seen. 
    int res = WSAIoctl(listenSock.sock, SIO_GET_EXTENSION_FUNCTION_POINTER
        , &GuidAcceptEx, sizeof(GuidAcceptEx), &lpfnAcceptEx, sizeof(lpfnAcceptEx)
        , &dwBytes, nullptr, nullptr);

    if (res == SOCKET_ERROR)
    {
        throw std::runtime_error("Can't get pointer: " + WSAGetLastError());
    }

    SOCKET acc_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (acc_socket == INVALID_SOCKET)
    {
        throw std::runtime_error("Can't create listen socket: " + WSAGetLastError());
    }

    BOOL b = lpfnAcceptEx(listenSock.sock, acc_socket
        , accept_buf.data(), 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16
        , &dwBytes, &overlapped_ac);

    if (!b && WSAGetLastError() != WSA_IO_PENDING) {
        closesocket(acc_socket);
        throw std::runtime_error("AcceptEx failed: " + WSAGetLastError());
    }

    Client* cl = new Client(acc_socket);
    cl->id = ids++;
    lastAccepted = std::make_unique<client_context>(this, cl);
    try {
        IOCP.bind(acc_socket, lastAccepted.get());
    }
    catch (std::exception const&)
    {   
        lastAccepted.reset();
        throw;
    }
}


unsigned server::clients_count() noexcept
{
    return g_client_storage.clients_count();
}

void server::drop_client(client_context* cl) noexcept
{
    if (cl == nullptr) return;

    g_client_storage.detach_client(cl);
    return;
}

void server::handle_queue_request(std::shared_ptr<Client> const& client, DWORD dwBytesTransfered)
{
    auto pair = g_client_queue.pair_or_queue(client);
    if (pair)
    {
        //Pair found
        char t = g_client_queue.generate_random();
        client->set_theme(t);
        pair->set_theme(t);
        client->on_pair_found(pair);
        pair->on_pair_found(client);
    }
}

server::server(server_launch_params params)
{
    listenSock.bind_and_listen(params);
    acceptContext = std::make_unique<client_context>(this, nullptr);
    IOCP.bind(listenSock.sock, acceptContext.get());
    accept();

    std::cout << "All OK, waiting for the connections" << std::endl;
}

server::~server()
{
    std::cout << "Server is shutting down...\n";
}
