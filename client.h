#pragma once
#include <WinSock2.h>
#include <string>

#include "client_buffer.h"

//Operation codes for clients
//Server sends data to client
#define OP_SEND 1
//Server recievs data
#define OP_RECV 2
//Special codes
#define OP_KEEP_ALIVE 10
#define OP_ACCEPT 11
#define OP_DELETED 12

//Client statuses
#define STATE_NEW 0
#define STATE_INIT 1
#define STATE_QUEUED 2
#define STATE_MESSAGING 4
#define STATE_VOTING 8

//Message types
#define MST_QUEUE 1
#define MST_MESSAGE 2
#define MST_TIMEOUT 3
#define MST_VOTING 4
#define MST_DISCONNECT 10
#define MST_LEAVE 69

//Maximum inactivity time in SECONDS
#ifdef _DEBUG
#define MAX_IDLENESS_TIME 15
#else
#define MAX_IDLENESS_TIME (10*60)
#endif

struct OVERLAPPED_EX : OVERLAPPED
{
    unsigned operation_code;

    explicit OVERLAPPED_EX(unsigned code) : OVERLAPPED{}, operation_code(code){};
};

class Client
{
    //Field, necessary for overlapped (async) operations
    OVERLAPPED_EX *overlapped_send;
    OVERLAPPED_EX *overlapped_recv;
    //Special field for keep-alive
    OVERLAPPED_EX *overlapped_special;

    //Buffer with data
    CLIENT_BUFFER buffer;

    //Socket of client
    SOCKET socket;

    //Client* companion = nullptr;    
    std::weak_ptr<Client> companion;
public:
    inline OVERLAPPED_EX* get_overlapped()
    {
        return overlapped_special;
    }
    unsigned long id;
    int client_status;
    std::string q_msg;

    std::weak_ptr<Client> &get_companion();
    void set_companion(std::weak_ptr<Client>const&);
    bool has_companion() const;

    char* get_recv_buffer_data();
    SOCKET get_socket();

    //Remember, this will reset your buffer
    bool recieve() noexcept;
    bool send(std::string const&) noexcept;
    bool safe_comp_send(std::string const&) noexcept;

    bool send_leaved();
    bool send_bad_vote();
    bool send_greetings(unsigned int users_online);

    int get_recv_message_type() const;
    int get_snd_message_type() const;

    explicit Client(SOCKET s);
    Client(const Client& other) = delete;
    Client& operator=(const Client& other) = delete;
    ~Client();
};

struct client_context
{
    std::shared_ptr<Client> ptr; //Copies right by default?
    char dummy;
    uint64_t lastActivity;
    client_context(Client* a) :ptr(a), dummy(0),lastActivity(current_time()) {};
    client_context() : dummy(1) {};

    client_context(const client_context& other) = delete;

    client_context& operator=(const client_context& other) = delete;

    inline bool isAlive() const noexcept
    {
        return (current_time() - lastActivity) < MAX_IDLENESS_TIME;
    }

    inline void updateTimer() noexcept
    {
        lastActivity = current_time();
    }
    inline std::function<void()> get_upd_f(HANDLE comp_port) noexcept
    {
        //Save overlapped as member of anonymous class, because THIS can be deleted at moment of run operator()
        LPOVERLAPPED over_ptr = (LPOVERLAPPED)this->ptr->get_overlapped();
        return [=] ()
        {
            PostQueuedCompletionStatus(comp_port, 1, (ULONG_PTR)this, over_ptr);
        };
    }
};

