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
    OVERLAPPED_EX* snd;
    OVERLAPPED_EX* rcv;
public:
    CLIENT_BUFFER buf;
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

enum client_statuses
{
    NEW, INIT, MESSAGING, VOTING
};

//Message types
#define MST_QUEUE 1
#define MST_MESSAGE 2
#define MST_TIMEOUT 3
#define MST_VOTING 4
#define MST_DISCONNECT 10
#define MST_LEAVE 69

class Client
{
    socket_user handle;
    std::string q_msg;
    std::weak_ptr<Client> companion;
    client_statuses status;

    int get_recv_message_type() const;
    int get_snd_message_type() const;

    bool safe_comp_send(std::string const&);
    bool send_leaved();
    bool send_bad_vote();

public:
    unsigned id;
    explicit Client(SOCKET s) : handle(s) { status = NEW; };
    client_res on_recv_finished(unsigned bytesTransfered);
    client_res on_send_finished(unsigned bytesTransfered);
    bool send_greetings(unsigned);
    SOCKET get_socket() { return handle.get_socket(); };

    void on_pair_found(std::shared_ptr<Client>const& pair);
    void set_theme(char);
    
    //~Client() {};
};

#ifdef _DEBUG
#define MAX_IDLENESS_TIME 15
#else
#define MAX_IDLENESS_TIME (10*60)
#endif

class client_context
{
    server* host;

    uint64_t lastActivity;
    OVERLAPPED_EX* over;
public:
    
    std::shared_ptr<Client> ptr;
    void on_overlapped_io_finished(unsigned bytesTransfered, OVERLAPPED_EX* overlapped) noexcept;
    explicit client_context(server* serv, Client* cl);
    ~client_context();

    bool isAlive() const noexcept;
    void updateTimer() noexcept;
    std::function<void()> get_upd_f(HANDLE comp_port) noexcept;
};