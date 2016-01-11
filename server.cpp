#include "stdafx.h"
#include "server.h"

namespace
{
    int calc_proc_count()
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        return si.dwNumberOfProcessors;
    }

    int get_proc_count()
    {
        static int const value = calc_proc_count();
        return value;
    }
}

DWORD server::WorkerThread(LPVOID param)
{
    server* me = static_cast<server*>(param);
    void* void_context = nullptr;
    OVERLAPPED* overlapped = nullptr;
    OVERLAPPED_EX* overlapped_ex;
    DWORD dwBytesTransfered = 0;

    while (true)
    {
        BOOL queued_result = GetQueuedCompletionStatus(me->g_io_completion_port, &dwBytesTransfered, (PULONG_PTR)&void_context, &overlapped, INFINITE);

        if (!queued_result)
        {
            auto err_code = GetLastError();
            std::cout << "Error dequeing: " << err_code << std::endl; //121 = ERROR_SEM_TIMEOUT; 64 = NET_NAME_INVALID, need to delete client
            if (err_code == ERROR_SEM_TIMEOUT || err_code == ERROR_NETNAME_DELETED)
            {
                me->drop_client(static_cast<client_context*>(void_context));
            }
            if(err_code == ERROR_CONNECTION_ABORTED) //Deleted socket
            {
                delete overlapped;
            }
            continue;
        }

        if (void_context == nullptr) //Signal to shutdown
        {
            return 0;
        }

        overlapped_ex = static_cast<OVERLAPPED_EX*>(overlapped);
        //Checking keep-alive
        if (overlapped_ex->op_code == operation_code::DELETED)
        {
            std::cout << "overlapped deleted\n";
            delete overlapped_ex;
            continue;
        }
        client_context* context = static_cast<client_context*>(void_context);
        if (overlapped_ex->op_code == operation_code::KEEP_ALIVE)
        {
            if (!context->isAlive())
            {
                std::cout << context->ptr->id << " idleness time exceeded" <<std::endl;
                delete overlapped_ex;
                me->drop_client(context);
            }
            else
            {
                //std::cout << "Adding delay...\n";
                //post another kill delay...
                me->g_func_queue.enqueue(context->get_upd_f(me->g_io_completion_port), MAX_IDLENESS_TIME);
            }
            continue;
        }
        context->updateTimer();
        auto client = context->ptr;
        //Checking if event is accept
        if (context->dummy)
        {
            if (me->lastAccepted != nullptr) {
                me->finish_accept();
            }
            else
            {
                std::cout << "WTF\n";
            }
            continue;
        }

        if (dwBytesTransfered == 0) //Client dropped connection
        {
            std::cout << client->id << " Zero bytes transfered, disconnecting (#" << std::this_thread::get_id() << std::endl;
            me->drop_client(context);
            continue;
        }

        if (overlapped_ex->op_code == operation_code::RECV && client->get_recv_message_type() == MST_DISCONNECT)
        {
            std::cout << client->id << " send disconnect" << std::endl;
            me->drop_client(context);
            continue;
        }

        switch (client->client_status)
        {
        case STATE_NEW:
            //This client has just received greetings. Therefore, this operation can be only SEND
            //Switch client to receive to get QUEUE request
            client->client_status = STATE_INIT;
            if (!client->recieve())
            {
                std::cout << "Error occurred while executing WSARecv: " << WSAGetLastError() << std::endl;
                //Let's not work with this client
                me->drop_client(context);
                break;
            }

            break;
        case STATE_INIT:
            //In this state, client may ask to queue him (operation_code::RECV) or to get pair (operation_code::SEND)
            if (overlapped_ex->op_code == operation_code::RECV)
            {
                if (client->get_recv_message_type() != MST_QUEUE || client->q_msg.size() > 0) //Seconds cond means it is already in queue
                {
                    client->recieve(); // If you ignore it, maybe it will go away
                    break;
                }

                //Making a pair for our client
                me->handle_queue_request(client, dwBytesTransfered);
                client->recieve(); //Client may wish to disconnect
            }
            else
            {
                //Clients has received their themes and started messaging
                //Change only one client, 'cause we'll got two SEND complete statuses
                if (client->get_snd_message_type()== MST_QUEUE)
                {
                    client->q_msg.resize(0);
                    client->client_status = STATE_MESSAGING;
                }
            }
            break;
        case STATE_MESSAGING:
            //Client just sending and receiving

            if (overlapped_ex->op_code == operation_code::RECV)
            {
                //We have received smth, let's send it to another client
                std::string msg(client->get_recv_buffer_data(), dwBytesTransfered);
                std::cout << client->id << " messaged " << msg << std::endl;

                //Change state if msg timeout or leave
                if (client->get_recv_message_type() == MST_TIMEOUT)
                    client->client_status = STATE_VOTING;
                if (client->get_recv_message_type() == MST_LEAVE)
                    client->client_status = STATE_INIT;

                if (!client->safe_comp_send(msg))
                {
                    //Handling UNEXPECTEDLY leave
                    client->send_leaved();
                }

                client->recieve();
            }
            else
            {
                // This client is already on receive, just got the message from companion
                if (client->get_snd_message_type() == MST_TIMEOUT)
                    client->client_status = STATE_VOTING;
                if (client->get_snd_message_type() == MST_LEAVE)
                    client->client_status = STATE_INIT;
            }
            break;
        case STATE_VOTING:
            //In this state, clients are only allowed to send or receive one message with results

            if (overlapped_ex->op_code == operation_code::RECV)
            {
                if (client->get_recv_message_type() != MST_VOTING)
                {
                    client->recieve();
                    break;
                }
                std::string msg(client->get_recv_buffer_data(), dwBytesTransfered);
                std::cout << client->id << " voted " << msg << std::endl;

                client->client_status = STATE_INIT;
                if (!client->safe_comp_send(msg)) client->send_bad_vote();
                client->set_companion(std::weak_ptr<Client>());
                client->recieve();
            }
            else
            {
                //Actually, nothing to do.
            }

            break;
        }
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

        try 
        {
            this->g_client_storage.attach_client(this->lastAccepted);
        } 
        catch (std::exception const&)
        {
            delete this->lastAccepted;
            throw;
        }
        if (!accepted->send_greetings(this->g_client_storage.clients_count()))
        {
            std::cout << "Error in inital send,"<<WSAGetLastError() << " drop client " << accepted->id << std::endl;
            this->drop_client(this->lastAccepted);
        }
        g_func_queue.enqueue(lastAccepted->get_upd_f(g_io_completion_port), MAX_IDLENESS_TIME);
        this->lastAccepted = nullptr;
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
    int res = WSAIoctl(listenSock, SIO_GET_EXTENSION_FUNCTION_POINTER
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

    BOOL b = lpfnAcceptEx(listenSock, acc_socket
        , accept_buf, 0, sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16
        , &dwBytes, overlapped_ac);

    if (!b && WSAGetLastError() != WSA_IO_PENDING) {
        std::cout << "AcceptEx failed: " << WSAGetLastError() << std::endl;
        return false;
    }

    Client* cl = new Client(acc_socket);
    cl->id = ids++;
    this->lastAccepted = new client_context(cl);
    HANDLE port = CreateIoCompletionPort((HANDLE)acc_socket, this->g_io_completion_port, (ULONG_PTR)this->lastAccepted, 0);
    if (port == nullptr)
    {
        std::cout << "Can't bind new client to comp port: " << GetLastError() << std::endl;
        this->lastAccepted = nullptr;
        delete cl;
        return false;
    }
    return true;
}

SOCKET server::create_listen_socket(server_launch_params params)
{
    //Creating overlapped socket
    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET)
    {
        std::cout << "Error opening socket"<< WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    int res = bind(sock, reinterpret_cast<sockaddr*>(&params.serv_address), sizeof(params.serv_address));
    if (res == SOCKET_ERROR)
    {
        std::cout << "Bind error" << WSAGetLastError() << std::endl;
        closesocket(sock);
        return INVALID_SOCKET;
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cout << "Listen failed with error:" << WSAGetLastError() << std::endl;
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
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
    client->q_msg = std::string(client->get_recv_buffer_data(), dwBytesTransfered);
    std::cout << client->id << "Q_MSG:" << client->q_msg << std::endl;

    auto pair = g_client_queue.pair_or_queue(client);
    if (pair)
    {
        //Pair found
        std::string msg1{ std::move(client->q_msg) };
        std::string msg2{ std::move(pair->q_msg) }; //Caching strings
        client->send(msg2);
        pair->send(msg1);
    }
}

bool server::init()
{
    //Initializing WSA
    WSADATA wsaData = { 0 };
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0)
    {
        printf("WSAStartup failed: %d\n", iResult);
        return false;
    }

    //Initializing IOCP
    g_io_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (g_io_completion_port == nullptr)
    {
        printf("IOCompletionPort init failed: %d\n", WSAGetLastError());
        WSACleanup();
        return false;
    }

    //Creating threads
#ifndef _DEBUG
    g_workers_count = g_worker_threads_per_processor * get_proc_count();
#else
    g_workers_count = 2;
#endif

    std::cout << "Threads count: " << g_workers_count << std::endl;
    g_worker_threads = new HANDLE[g_workers_count];

    for (auto i = 0; i < g_workers_count;i++)
    {
        g_worker_threads[i] = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
    }
    return true;
}

void server::shutdown()
{
    std::cout << "Server is shutting down...\n";
    for (int i = 0;i < g_workers_count; i++)
    {
        //Signal for threads - if they get NULL context, they shutdown.
        PostQueuedCompletionStatus(g_io_completion_port, 0, 0, nullptr);

    }
    //Waiting threads to shutdown
    WaitForMultipleObjects(g_workers_count, g_worker_threads, true, INFINITE);
    g_client_storage.clear_all();
    CloseHandle(g_io_completion_port);
    closesocket(listenSock);
    delete[] g_worker_threads;
    WSACleanup();
}

server::server(server_launch_params params)
{
    overlapped_ac = new OVERLAPPED_EX{operation_code::ACCEPT};
    accept_buf = static_cast<char*>(malloc(sizeof(char)*(2 * sizeof(sockaddr_in) + 32)));
    if (!init()) throw std::runtime_error("Error in initializing");
    listenSock = create_listen_socket(params);
    if (listenSock == INVALID_SOCKET)
    {
        std::cout << "Failed to create listen socket\n";
        shutdown();
        throw std::runtime_error("Cannot create listen socket");
    }

    acceptContext = new client_context();
    CreateIoCompletionPort((HANDLE)listenSock, g_io_completion_port, (ULONG_PTR)acceptContext, 0);
    if (!this->accept())
    {
        throw std::exception("Cannot accept");
    }

    std::cout << "All OK, waiting for the connections" << std::endl;
}

server::~server()
{
    shutdown();
    delete acceptContext;
    delete lastAccepted;
    delete overlapped_ac;
    free(accept_buf);
}
