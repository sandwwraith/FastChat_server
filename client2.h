#pragma once
#include <WinSock2.h>
#include <string>

#include "client_buffer.h"

//Operation codes for clients
enum class operation_code : unsigned
{
    //Server sends data to client
    SEND = 1,
    //Server recievs data
    RECV = 2,
    //Special codes
    KEEP_ALIVE = 10,
    ACCEPT = 11,
    DELETED = 12,
};

struct OVERLAPPED_EX : OVERLAPPED
{
    operation_code op_code;

    explicit OVERLAPPED_EX(operation_code op_code) : OVERLAPPED{}, op_code(op_code){}
};

class socket_user
{
private:
    SOCKET sock;
    CLIENT_BUFFER buf;
    OVERLAPPED_EX* snd;
    OVERLAPPED_EX* rcv;
public:
    void send(std::string const&);
    void recv();
    std::string read(unsigned bytes_count);
    explicit socket_user(SOCKET s);
    ~socket_user();
    //socket_user();
    socket_user(const socket_user& other) = delete;
    socket_user& operator=(const socket_user& other) = delete;
    SOCKET get_socket() { return sock; };
};

class server;

enum client_res
{
    OK, DISCONNECT, QUEUE_OP
};
class Client
{
    socket_user handle;
public:
    unsigned id;
    explicit Client(SOCKET s) : handle(s) {};
    client_res on_recv_finished(unsigned bytesTransfered);
    client_res on_send_finished(unsigned bytesTransfered);
    bool send_greetings(unsigned);
    SOCKET get_socket() { return handle.get_socket(); };
    //~Client() {};
};


class client_context
{
    server* host;
public:
    std::shared_ptr<Client> ptr;
    void on_overlapped_io_finished(unsigned bytesTransfered, OVERLAPPED_EX* overlapped) noexcept;
    explicit client_context(server* serv, Client* cl) : host(serv), ptr(cl) {};
};