#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <mswsock.h>
#include <string>
#include <iostream>
#include <conio.h>

#include "client_queue.h"
#include "client_storage.h"
#include "function_queue.h"
#include "wrappers.h"

#pragma comment(lib,"Ws2_32.lib")

class server
{
    friend class client_context;
    friend class ThreadPool;

    std::unique_ptr<client_context> lastAccepted;
    std::unique_ptr<client_context> acceptContext;

    WSAWrapper wsa_wrapper{};
    IOCPWrapper IOCP{};
    ListenSocketWrapper listenSock;

    //Global storage for all clients
    client_storage g_client_storage;
    client_queue g_client_queue;
    function_queue g_func_queue;

    static DWORD WINAPI WorkerThread(LPVOID); //Worker function for threads
    ThreadPool pool{ this };

    OVERLAPPED_EX overlapped_ac{ operation_code::ACCEPT };
    std::array<char, sizeof(char)*(2 * sizeof(sockaddr_in) + 32)> accept_buf;

    bool accept();
    void finish_accept() noexcept;
    unsigned long ids = 0;

    void drop_client(client_context*) noexcept;
    
    void handle_queue_request(std::shared_ptr<Client>const&, DWORD);

public:
    unsigned int clients_count() noexcept;
    explicit server(server_launch_params);
    server(const server& other) = delete;
    server& operator=(const server& other) = delete;
    ~server();
};

