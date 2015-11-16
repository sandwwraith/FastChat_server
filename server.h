#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <string>
#include <iostream>
#include <conio.h>

#include "client.h"
#include "client_storage.h"

#pragma comment(lib,"Ws2_32.lib")

#ifndef GLOBALS_AND_CONSTANTS_SERVER
#define GLOBALS_AND_CONSTANTS_SERVER

//Number of threads per processor
#define WORKER_THREADS_PER_PROCESSOR 2

//Operation codes for clients

#define OP_SEND 1
//Server sends data to client
#define OP_RECV 2
//Server recievs data

//Defaul greetings message. It will be concatinated with user address
#define STR_GREETINGS "Hello, user "
#endif


class server
{
private:
    //Global parameter for server port
    int g_server_port = 2539;

    //Global array of worker threads
    HANDLE* g_worker_threads = nullptr;

    //Number of threads
    int g_workers_count = 0;

    //Number of processors in system
    //TODO: get it from somewhere
    int g_processors_count = 4;

    //Main IOCP port
    HANDLE g_io_completion_port;

    //Global storage for all clients
    client_storage g_client_storage;

    static DWORD WINAPI WorkerThread(LPVOID); //Worker function for threads
    bool g_started = false;
    SOCKET create_listen_socket();

public:

    //Returns true if port was setted (false if server already started)
    bool set_port(int new_port);
    int get_port() const;

    server();
    //if init_now is true, inits server immedeately
    explicit server(bool init_now);
    bool init();

    int main_cycle();

    void shutdown();
    ~server();
};

