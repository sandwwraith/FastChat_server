#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <mswsock.h>
#include <vector>
#include "client2.h"

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

struct WSAWrapper
{
    inline WSAWrapper()
    {
        WSADATA wsaData = { 0 };
        int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult != 0)
        {
            throw std::exception("WSAStartup failed");
        }
    }
    
    inline ~WSAWrapper()
    {
        WSACleanup();
    }

    WSAWrapper(const WSAWrapper& other) = delete;
    WSAWrapper& operator=(const WSAWrapper& other) = delete;
};

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

class client_context;
class server;

struct ListenSocketWrapper
{
    SOCKET sock{ INVALID_SOCKET };
    inline ListenSocketWrapper()
    {
        sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
        if (sock == INVALID_SOCKET)
        {
            throw std::runtime_error("Error opening socket" + WSAGetLastError());
        }
    }

    inline void bind_and_listen(server_launch_params params)
    {
        int res = bind(sock, reinterpret_cast<sockaddr*>(&params.serv_address), sizeof(params.serv_address));
        if (res == SOCKET_ERROR)
        {
            throw std::runtime_error("Bind error " + WSAGetLastError());
        }
        if (listen(sock, SOMAXCONN) == SOCKET_ERROR)
        {
            throw std::runtime_error("Listen failed with error:" + WSAGetLastError());
        }
    }

    inline ~ListenSocketWrapper()
    {
        closesocket(sock);
    }

    ListenSocketWrapper(const ListenSocketWrapper& other) = delete;
    ListenSocketWrapper& operator=(const ListenSocketWrapper& other) = delete;
};

struct IOCPWrapper
{
private:
    HANDLE iocp_port;
public:
    inline bool Dequeue(client_context*& a,DWORD& dwBytesTransfered, OVERLAPPED*& oveer) noexcept
    {
        return GetQueuedCompletionStatus(iocp_port, &dwBytesTransfered, (PULONG_PTR)&a,&oveer, INFINITE) != 0;
    }

    inline IOCPWrapper()
    {
        iocp_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (iocp_port == nullptr)
        {
            throw std::runtime_error("IOCompletionPort init failed: " +  std::to_string(WSAGetLastError()));
        }
    }

    inline void bind(SOCKET socket, client_context* context)
    {
        if (CreateIoCompletionPort((HANDLE)socket, iocp_port, (ULONG_PTR)context, 0) == nullptr)
            throw std::runtime_error("cannot bind client to port!" + GetLastError());
    }

    inline void post(client_context* context, OVERLAPPED_EX* overlapped, DWORD bytes)
    {
        if (PostQueuedCompletionStatus(iocp_port, bytes, (ULONG_PTR)context, (LPOVERLAPPED)overlapped) == 0)
            throw std::runtime_error("Cannot post in IOCP: " + GetLastError());
    }

    inline ~IOCPWrapper()
    {
        // I think it is very strange to get an exception on a server destroy
        // According to documentation, the most likely case to fail is when passed
        // parameter is not a HANDLE or already closed.
        BOOL r = CloseHandle(iocp_port);
        assert (r != FALSE);
    }

    IOCPWrapper(const IOCPWrapper& other) = delete;
    IOCPWrapper& operator=(const IOCPWrapper& other) = delete;
};

class ThreadPool
{
    std::vector<HANDLE> threads;
    //Number of threads per processor
    int g_worker_threads_per_processor = 2;
    server* host;
public:
    explicit ThreadPool(server* host);
    void closethreads();

    inline ~ThreadPool()
    {
        closethreads();
    }

    ThreadPool(const ThreadPool& other) = delete;
    ThreadPool& operator=(const ThreadPool& other) = delete;
};
