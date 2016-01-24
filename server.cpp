#include "stdafx.h"
#include "server.h"

DWORD server::WorkerThread(LPVOID param)
{
    server* me = static_cast<server*>(param);
    void* void_context = nullptr;
    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED_EX* overlapped_ex;
    DWORD dwBytesTransfered = 0;
    while (true)
    {
        BOOL queued_result = GetQueuedCompletionStatus(me->IOCP.iocp_port, &dwBytesTransfered,
            (PULONG_PTR)&void_context, &overlapped, INFINITE);

        if (!queued_result)
        {
            auto err_code = GetLastError();
             //121 = ERROR_SEM_TIMEOUT; 64 = NET_NAME_INVALID, need to delete client
            if (err_code == ERROR_SEM_TIMEOUT || err_code == ERROR_NETNAME_DELETED)
            {
                me->drop_client(static_cast<client_context*>(void_context));
            }
            else if (err_code == ERROR_CONNECTION_ABORTED || err_code == ERROR_OPERATION_ABORTED) //Closed socket
            {
                delete overlapped;
            }
            else std::cout << "Error dequeing: " << err_code << std::endl;
            continue;
        }

        if (void_context == nullptr) //Signal to shutdown
        {
            return 0;
        }

        overlapped_ex = static_cast<OVERLAPPED_EX*>(overlapped);
        if (overlapped_ex->op_code == operation_code::DELETED)
        {
            delete overlapped_ex;
            continue;
        }

        // TODO: I have got question.
        // When we receive operation_code::KEEP_ALIVE we post another KEEP_ALIVE event.
        // Doesn't this lead to 100% one core CPU usage when one client is connected?
        client_context* context = static_cast<client_context*>(void_context);
        if (overlapped_ex->op_code == operation_code::KEEP_ALIVE)
        {
            if (!context->isAlive())
            {
                std::cout << context->ptr->id << " idleness time exceeded" << std::endl;
                delete overlapped_ex;
                me->drop_client(context);
            }
            else
            {
                //post another kill delay
                me->g_func_queue.enqueue(context->get_upd_f(me->IOCP.iocp_port), MAX_IDLENESS_TIME);
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
        this->g_client_storage.attach_client(std::move(this->lastAccepted));

        if (!accepted->send_greetings(this->g_client_storage.clients_count()))
        {
            std::cout << "Error in inital send,"<<WSAGetLastError() << " drop client " << accepted->id << std::endl;
            this->drop_client(curr_val);
        }
        else 
        {
            g_func_queue.enqueue(curr_val->get_upd_f(IOCP.iocp_port), MAX_IDLENESS_TIME);
        }
        
        if (!this->accept()) throw std::exception("Can't start accept, WSA code" + WSAGetLastError());
    }
    catch (std::exception const& ex)
    {
        std::cout << "Fatal ACCEPT error: " << ex.what() << std::endl;
        //WAT TO DO ???
    }
}


bool server::accept()
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
        std::cout << "Can't get pointer: " << WSAGetLastError() << std::endl;
        return false;
    }

    SOCKET acc_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (acc_socket == INVALID_SOCKET)
    {
        std::cout << "Can't create listen socket: " << WSAGetLastError() << std::endl;
        return false;
    }

    BOOL b = lpfnAcceptEx(listenSock.sock, acc_socket
        , accept_buf.data(), 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16
        , &dwBytes, &overlapped_ac);

    if (!b && WSAGetLastError() != WSA_IO_PENDING) {
        std::cout << "AcceptEx failed: " << WSAGetLastError() << std::endl;
        // TODO: shouldn't acc_socket be closed here?
        return false;
    }

    Client* cl = new Client(acc_socket);
    cl->id = ids++;
    lastAccepted = std::make_unique<client_context>(this, cl);
    try {
        IOCP.bind(acc_socket, lastAccepted.get());
    }
    catch (std::exception const& e)
    {   
        lastAccepted.reset();
        std::cout << "Can't bind new client to comp port: " << e.what()<< " " <<  GetLastError() << std::endl;
        return false;
    }
    return true;
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
    if (!this->accept())
    {
        throw std::exception("Cannot accept");
    }

    std::cout << "All OK, waiting for the connections" << std::endl;
}

server::~server()
{
    std::cout << "Server is shutting down...\n";
}
