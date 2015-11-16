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

//Global parameter for server port
int g_server_port = 2539;

//Global array of worker threads
HANDLE* g_worker_threads = nullptr;

//Number of threads
int g_workers_count = 0;

//Number of threads per processor
#define WORKER_THREADS_PER_PROCESSOR 2

//Number of processors in system
//TODO: get it from somewhere
int g_processors_count = 4;

//Main IOCP port
HANDLE g_io_completion_port;

//Forward decl for thread func
DWORD WINAPI WorkerThread(LPVOID);
void shutdown();

//Operation codes for clients
#define OP_SEND 1
    //Server sends data to client
#define OP_RECV 2
    //Server recievs data

//Defaul greetings message. It will be concatinated with user address
#define STR_GREETINGS "Hello, user "

//Global storage for all clients
client_storage g_client_storage;

#endif