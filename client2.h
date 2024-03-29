#pragma once
#include <WinSock2.h>
#include <string>

#include "client_buffer.h"
#include "overlapped_ex.h"
#include "overlapped_ptr.h"


class socket_user
{
    struct _socket_wrapper
    {
    private:
        SOCKET so;
    public:
        _socket_wrapper(SOCKET s) : so(s) {};
        inline ~_socket_wrapper() { closesocket(so); }
        operator SOCKET() { return so; }
    };
    //This wrapper structure is necessary for overlapped pointers
    // be deleted (or marked as) before the socket closure.

    _socket_wrapper sock;
    overlapped_ptr snd;
    overlapped_ptr rcv;
public:
    CLIENT_BUFFER snd_buf;
    CLIENT_BUFFER rcv_buf;
    void send(std::string const&);
    void recv();
    std::string read(unsigned bytes_count);
    explicit socket_user(SOCKET s);
    ~socket_user();
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

enum class message_type : char
{
    QUEUE = 1,
    MESSAGE = 2,
    TIMEOUT = 3,
    VOTING = 4,
    DISCONNECT = 10,
    LEAVE = 69
};

class Client
{
    socket_user handle;
    std::string q_msg;
    std::weak_ptr<Client> companion;
    client_statuses status;

    message_type get_recv_message_type() const;
    message_type get_snd_message_type() const;

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
};

constexpr static const int MAX_IDLENESS_TIME =
#ifdef _DEBUG
20
#else
(10 * 60)
#endif
;

class client_context
{
    server* host;
    uint64_t lastActivity;
public:
    std::shared_ptr<Client> ptr;
private:
    overlapped_ptr over;
public:
    
    void on_overlapped_io_finished(unsigned bytesTransfered, OVERLAPPED_EX* overlapped) noexcept;
    explicit client_context(server* serv, Client* cl);
    ~client_context();

    bool isAlive() const noexcept;
    void updateTimer() noexcept;
    std::function<void()> get_upd_f() noexcept;

    client_context(const client_context& other) = delete;
    client_context& operator=(const client_context& other) = delete;
};