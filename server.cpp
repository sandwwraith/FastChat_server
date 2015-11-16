#include "server.h"


DWORD server::WorkerThread(LPVOID param)
{
    server* me = (server*)param;
    void* context = NULL;
    OVERLAPPED* pOverlapped = NULL;
    DWORD dwBytesTransfered = 0;

    int snd;
    DWORD dwBytes = 0, dwFlags = 0;

    while (true)
    {
        BOOL res = GetQueuedCompletionStatus(me->g_io_completion_port, &dwBytesTransfered, (PULONG_PTR)&context, &pOverlapped, INFINITE);
        if (!res)
        {
            //Concurrency issues
            std::cout << "Error dequeing: " << GetLastError() << std::endl;
            continue;
        }
        if (context == nullptr) //Signal to shutdown
        {
            return 0;
        }
        Client* client = static_cast<Client*>(context);
        if (dwBytesTransfered == 0) //Client dropped connection
        {
            me->g_client_storage.detach_client(client);
            continue;
        }

        //TODO: Implement working on opcodes
        std::string data;
        switch (client->op_code)
        {
        case OP_SEND:
            //We have sent something to client, let's see what he responded
            dwFlags = 0;
            client->reset_buffer();
            client->client_message.clear();
            snd = WSARecv(client->get_socket(), client->get_wsabuff_ptr(), 1, &dwBytes, &dwFlags, client->get_overlapped_ptr(), nullptr);

            if ((SOCKET_ERROR == snd) && (WSA_IO_PENDING != WSAGetLastError()))
            {
                printf("\nThread: Error occurred while executing WSARecv (%d).", WSAGetLastError());

                //Let's not work with this client
                me->g_client_storage.detach_client(client);
                continue;
            }
            client->op_code = OP_RECV;
            break;
        case OP_RECV:
            //Client sent to us something
            data = client->get_buffer_data();
#ifdef _DEBUG
            std::cout << "Client sent " << data << std::endl;
#endif
            client->client_message.append(data);
            if (data[data.size() - 1] == 3)
            {
                me->g_client_storage.detach_client(client);
                continue;
            }
            dwFlags = 0;
            if (data[data.size() - 1] >= 32 || data[data.size() - 1] == 8 /*backspace. TODO: Improve*/)
            {
                //Waiting for control symbol
                client->reset_buffer();
                snd = WSARecv(client->get_socket(), client->get_wsabuff_ptr(), 1, &dwBytes, &dwFlags, client->get_overlapped_ptr(), nullptr);
                client->op_code = OP_RECV;
            }
            else
            {
                //Respond to him
                client->reset_buffer();
                CopyMemory(client->get_buffer_data(), client->client_message.c_str(), client->client_message.length());
                client->get_wsabuff_ptr()->len = client->client_message.length();
                snd = WSASend(client->get_socket(), client->get_wsabuff_ptr(), 1, &dwBytes, dwFlags, client->get_overlapped_ptr(), nullptr);
                client->op_code = OP_SEND;
            }
            if ((SOCKET_ERROR == snd) && (WSA_IO_PENDING != WSAGetLastError()))
            {
                printf("\nThread: Error occurred while executing WSASend (%d).", WSAGetLastError());

                //Let's not work with this client
                me->g_client_storage.detach_client(client);
                continue;
            }
            break;
        default:
            std::cout << "Error: undefined opcode\n";
            continue;
        }
    }
}

SOCKET server::create_listen_socket()
{
    //Creating overlapped socket
    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == INVALID_SOCKET)
    {
        std::cout << "Error opening socket\n";
        return INVALID_SOCKET;
    }

    sockaddr_in serv_address;
    //serv_address.sin_addr.S_un.S_addr = INADDR_ANY;
    serv_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    serv_address.sin_family = AF_INET;
    serv_address.sin_port = htons(g_server_port);

    int res = bind(sock, reinterpret_cast<sockaddr*>(&serv_address), sizeof(serv_address));
    if (res == SOCKET_ERROR)
    {
        std::cout << "bind error\n";
        closesocket(sock);
        return INVALID_SOCKET;
    }
    if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
    {
        printf("Listen failed with error: %d\n", WSAGetLastError());
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
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
        return false;
    }

    //Creating threads
    g_workers_count = WORKER_THREADS_PER_PROCESSOR * g_processors_count;
    g_worker_threads = new HANDLE[g_workers_count];

    for (auto i = 0; i < g_workers_count;i++)
    {
        g_worker_threads[i] = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
    }
    return true;
}

int server::main_cycle()
{
    if (!g_started && !init())
    {
        std::cout << "Failed to init\n";
        return 1;
    }
    SOCKET sock = create_listen_socket();
    if (sock == INVALID_SOCKET)
    {
        std::cout << "Failed to create listen socket\n";
        shutdown();
        return 1;
    }
    //Switching socket to non-blocking mode (to perform operations on main thread)
    unsigned long anb = 1;
    ioctlsocket(sock, FIONBIO, &anb);

    std::cout << "All OK, waiting for the connections" << std::endl;
    std::cout << "Press any key to exit" << std::endl;
    while (!_kbhit()) {

        //Accepting new client

        sockaddr_in client_address;
        int cl_length = sizeof(client_address);
        SOCKET accepted = WSAAccept(sock, reinterpret_cast<sockaddr*>(&client_address), &cl_length, nullptr, 0);
        //SOCKET accepted = accept(sock, (sockaddr*)&client_address, &cl_length);
        if (accepted == INVALID_SOCKET)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                printf("Accept failed with error: %d\n", WSAGetLastError());
            continue;
        }

        std::string client_name = inet_ntoa(client_address.sin_addr);
        std::cout << "Connected: " << client_name << std::endl;
        Client* client = new Client(accepted);
        g_client_storage.attach_client(client);

        //Associating client with IOCP
        if (CreateIoCompletionPort((HANDLE)client->get_socket(), g_io_completion_port, (ULONG_PTR)client, 0) == nullptr)
        {
            std::cout << "Error linking client to completion port\n";
            g_client_storage.detach_client(client);
            continue;
        }

        //Sending greetings
        DWORD dwFlags = 0;
        DWORD dwBytes = 0;
        std::string greetings = STR_GREETINGS + client_name + "!\r\n";

        client->reset_buffer();
        WSABUF* bff = client->get_wsabuff_ptr();
        CopyMemory(bff->buf, greetings.c_str(), greetings.length());
        bff->len = greetings.length();
        int res = WSASend(client->get_socket(), bff, 1, &dwBytes, dwFlags, client->get_overlapped_ptr(), nullptr);
        int lasterr = WSAGetLastError();
        if ((SOCKET_ERROR == res) && (WSA_IO_PENDING != lasterr))
        {
            printf("\nError in Initial send. %d\n", lasterr);
            g_client_storage.detach_client(client);
            continue;
        }
        client->op_code = OP_SEND;
    }
    shutdown();
    return 0;
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
    delete[] g_worker_threads;
    WSACleanup();
    g_started = false;
}

bool server::set_port(int new_port)
{
    if (g_started) return false;
    g_server_port = new_port;
    return true;
}

int server::get_port() const
{
    return g_server_port;
}

server::server() : server(false)
{
}

server::server(bool init_now)
{
    if (init_now)
    {
        g_started = init();
    }
}

server::~server()
{
    if (g_started) shutdown();
}
