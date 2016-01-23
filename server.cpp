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
        BOOL queued_result = GetQueuedCompletionStatus(me->g_io_completion_port, &dwBytesTransfered,
            (PULONG_PTR)&void_context, &overlapped, INFINITE);

        if (!queued_result)
        {
            auto err_code = GetLastError();
            std::cout << "Error dequeing: " << err_code << std::endl; //121 = ERROR_SEM_TIMEOUT; 64 = NET_NAME_INVALID, need to delete client
            if (err_code == ERROR_SEM_TIMEOUT || err_code == ERROR_NETNAME_DELETED)
            {
                me->drop_client(static_cast<client_context*>(void_context));
            }
            if (err_code == ERROR_CONNECTION_ABORTED) //Deleted socket
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
        if (overlapped_ex->op_code == operation_code::DELETED)
        {
            delete overlapped_ex;
            continue;
        }

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
                //post another kill delay...
                me->g_func_queue.enqueue(context->get_upd_f(me->g_io_completion_port), MAX_IDLENESS_TIME);
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
        else 
        {
            g_func_queue.enqueue(lastAccepted->get_upd_f(g_io_completion_port), MAX_IDLENESS_TIME);
        }
        
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
    this->lastAccepted = new client_context(this,cl);
    HANDLE port = CreateIoCompletionPort((HANDLE)acc_socket, this->g_io_completion_port, (ULONG_PTR)this->lastAccepted, 0);
    if (port == nullptr)
    {
        std::cout << "Can't bind new client to comp port: " << GetLastError() << std::endl;
        delete lastAccepted;
        this->lastAccepted = nullptr;
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
    // TODO: replace g_worker_threads and g_workers_count with vector
    g_worker_threads = new HANDLE[g_workers_count];

    for (auto i = 0; i < g_workers_count;i++)
    {
        // TODO: check error code
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
    // TODO: move this throw inside init()
    if (!init()) throw std::runtime_error("Error in initializing");
    listenSock = create_listen_socket(params);
    if (listenSock == INVALID_SOCKET)
    {
        std::cout << "Failed to create listen socket\n";
        shutdown();
        throw std::runtime_error("Cannot create listen socket");
    }

    acceptContext = new client_context(this, nullptr);
    // TODO: check error code
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
