#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <mswsock.h>
#include <string>
#include <iostream>
#include <conio.h>

#include "client_queue.h"
#include "client.h"
#include "client_storage.h"

#pragma comment(lib,"Ws2_32.lib")

struct server_launch_params
{
    sockaddr_in serv_address;

    explicit server_launch_params(bool global)
    {
        if (global)
            serv_address.sin_addr.S_un.S_addr = INADDR_ANY;
        else
            serv_address.sin_addr.s_addr = inet_addr("127.0.0.1");

        serv_address.sin_family = AF_INET;
        serv_address.sin_port = htons(2539);
    }

    void set_port(int port)
    {
        serv_address.sin_port = htons(port);
    }
};

class server
{
    //Global parameter for server address/port
    int g_server_port = 2539;

    //If true, recieves on INADDR_ANY. localhost otherwise.
    bool global_addr = false;

    //Global array of worker threads
    HANDLE* g_worker_threads = nullptr;

    //Number of threads
    int g_workers_count = 0;

    //Number of processors in system
    int g_processors_count = -1;

    //Number of threads per processor
    int g_worker_threads_per_processor = 2;

    //Main IOCP port
    HANDLE g_io_completion_port;

    //Global storage for all clients
    client_queue g_client_queue;
    client_storage g_client_storage;

    static DWORD WINAPI WorkerThread(LPVOID); //Worker function for threads
    bool init();
    void shutdown();

    SOCKET create_listen_socket(server_launch_params);
    SOCKET listenSock;

    OVERLAPPED* overlapped_ac;
    char* accept_buf;
    Client* lastAccepted = nullptr;
    Client* acceptContext = nullptr;

    bool accept();
    unsigned long ids = 0;

    void drop_client(Client*);
public:

    int get_proc_count();
    unsigned int clients_count() const;
    explicit server(server_launch_params);
    ~server();
};

