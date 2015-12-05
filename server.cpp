#include "server.h"
#include <thread>


DWORD server::WorkerThread(LPVOID param)
{
    server* me = static_cast<server*>(param);
    void* context = nullptr;
    OVERLAPPED* pOverlapped = nullptr;
    DWORD dwBytesTransfered = 0;

    while (true)
    {
        BOOL queued_result = GetQueuedCompletionStatus(me->g_io_completion_port, &dwBytesTransfered, (PULONG_PTR)&context, &pOverlapped, INFINITE);
        if (!queued_result)
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
            std::cout << client->id << " Zero bytes transfered, disconnecting (#" << std::this_thread::get_id() << std::endl;
            me->g_client_storage.detach_client(client);
            continue;
        }

        //TODO: Handle leave everywhere
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
                me->g_client_storage.detach_client(client);
                break;
            }

            break;
        case STATE_INIT:
            //In this state, client may ask to queue him (OP_RECV) or to get pair (OP_SEND)
            if (client->op_code == OP_RECV)
            {
                //Making a pair for our client
                if (me->g_client_queue.size() >= 1)
                {
                    me->g_client_queue.make_pair(client);
                    //Send them info
                    std::string msg1(client->get_buffer_data(), dwBytesTransfered);
                    std::string msg2(client->companion->get_buffer_data(), dwBytesTransfered);
                    client->send(msg2);
                    client->companion->send(msg1);
                }
                else
                {
                    //Enqueue user
                    me->g_client_queue.push(client);
                }
            }
            else
            {
                //Clients has received their themes and started messaging
                //Change only one client, 'cause we'll got two SEND complete statuses
                client->client_status = STATE_MESSAGING;
                client->recieve();
            }
            break;
        case STATE_MESSAGING:
            //Client just sending and receiving

            if (client->op_code == OP_RECV)
            {
                //We have received smth, let's send it to another client
                std::string msg(client->get_buffer_data(), dwBytesTransfered);
                std::cout << client->id << " dequed " << msg << std::endl;

                //Change state if msg timeout or disconnect
                if (client->get_message_type() == MST_TIMEOUT)
                    client->client_status = STATE_VOTING;
                if (client->companion->client_status == STATE_MESSAGING) //Concurrency... ?
                    client->companion->send(msg);
                client->recieve();
            }
            else
            {
                // This client is already on receive, just got the message from companion
                if (client->get_message_type() == MST_TIMEOUT)
                    client->client_status = STATE_VOTING;
                client->reset_buffer();
                client->op_code = OP_RECV;
                //Too easy. Look suspicious...
            }
            break;
        case STATE_VOTING:
            //In this state, clients are only allowed to send or receive one message with results

            if (client->op_code == OP_RECV)
            {
                std::string msg(client->get_buffer_data(), dwBytesTransfered);
                std::cout << client->id << " voted " << msg << std::endl;
                client->companion->send(msg);

                client->client_status = STATE_FINISHED;
            }
            else
            {
                //If this client received, but didn't voted itself
                client->reset_buffer();
                client->op_code = OP_RECV;

                client->client_status = STATE_FINISHED;
            }

            break;
        case STATE_FINISHED:
            //In this state, client who send first, received pair message and resets
            //Client who send second delivers his message to first and also resets

            if (client->op_code == OP_RECV)
            {
                std::string msg(client->get_buffer_data(), dwBytesTransfered);
                std::cout << client->id << " voted " << msg << std::endl;
                client->companion->send(msg);
            }

            //Client may want to find another pair
            client->companion = nullptr;
            client->client_status = STATE_INIT;
            client->recieve();

            break;
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

    if (global_addr)
        serv_address.sin_addr.S_un.S_addr = INADDR_ANY;
    else 
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

int server::get_proc_count()
{
    if (g_processors_count == -1)
    {
        //Getting system info
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        g_processors_count = si.dwNumberOfProcessors;
    }
    return g_processors_count;
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
    //g_workers_count = g_worker_threads_per_processor * get_proc_count();
    g_workers_count = 2;
    std::cout << "Threads count: " << g_workers_count << std::endl;
    g_worker_threads = new HANDLE[g_workers_count];

    for (auto i = 0; i < g_workers_count;i++)
    {
        g_worker_threads[i] = CreateThread(nullptr, 0, WorkerThread, this, 0, nullptr);
    }
    g_started = true;
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

    int count = 0;
    while (!_kbhit()) {

        //Accepting new client

        sockaddr_in client_address;
        int cl_length = sizeof(client_address);
        SOCKET accepted = WSAAccept(sock, reinterpret_cast<sockaddr*>(&client_address), &cl_length, nullptr, 0);
        if (accepted == INVALID_SOCKET)
        {
            if (WSAGetLastError() != WSAEWOULDBLOCK)
                printf("Accept failed with error: %d\n", WSAGetLastError());
            continue;
        }

        std::string client_name = inet_ntoa(client_address.sin_addr);
        std::cout << "Connected: " << client_name << std::endl;
        Client* client = new Client(accepted);
        client->id = count++;
        g_client_storage.attach_client(client);

        //Associating client with IOCP
        if (CreateIoCompletionPort((HANDLE)client->get_socket(), g_io_completion_port, (ULONG_PTR)client, 0) == nullptr)
        {
            std::cout << "Error linking client to completion port\n";
            g_client_storage.detach_client(client);
            continue;
        }

        //Sending greetings
        std::string greetings = STR_GREETINGS + client_name + "!\r\n";

        if (!client->send(greetings))
        {
            printf("\nError in Initial send. %d\n", WSAGetLastError());
            g_client_storage.detach_client(client);
            continue;
        }
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

int server::get_worker_threads_per_processor() const
{
    return g_worker_threads_per_processor;
}

bool server::set_worker_threads_per_processor(int g_worker_threads_per_processor1)
{
    if (g_started) return false;
    g_worker_threads_per_processor = g_worker_threads_per_processor1;
    return true;
}

bool server::set_port(int new_port)
{
    if (g_started) return false;
    g_server_port = new_port;
    return true;
}

bool server::is_addr_global() const
{
    return global_addr;
}

bool server::set_global_addr(bool set_to_global)
{
    if (g_started) return false;
    global_addr = set_to_global;
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
