#pragma once
#include <WinSock2.h>
#include <string>
#include<atomic>
//Maximum buffer size for I/O Operations
#define MAX_BUFFER_SIZE 256

//Operation codes for clients
#define OP_SEND 1
//Server sends data to client
#define OP_RECV 2
//Server recievs data

//Client statuses
#define DUMMY -1
#define STATE_NEW 0
#define STATE_INIT 1
#define STATE_QUEUED 2
#define STATE_MESSAGING 4
#define STATE_VOTING 8
#define STATE_FINISHED 16

//Message types
#define MST_QUEUE 1
#define MST_MESSAGE 2
#define MST_TIMEOUT 3
#define MST_VOTING 4
#define MST_DISCONNECT 10
#define MST_LEAVE 69

struct OVERLAPPED_EX : OVERLAPPED
{
    unsigned operation_code;
};
class Client
{
    //Field, necessary for overlapped (async) operations
    OVERLAPPED_EX *overlapped_send;
    OVERLAPPED_EX *overlapped_recv;

    //Buffer with data
    WSABUF *wsabuf;

    //Socket of client
    SOCKET socket;

    CRITICAL_SECTION cl_sec;

    Client* companion = nullptr;    
public:
    int op_code;
    unsigned long id;
    int client_status;
    std::string q_msg;

    Client* get_companion() const;
    void set_companion(Client*);
    void delete_companion();
    bool has_companion();
    bool own_companion();

    char* get_buffer_data();
    int get_buffer_size() const;
    WSABUF* get_wsabuff_ptr();
    void reset_buffer();
    SOCKET get_socket();

    //Remember, this will reset your buffer
    bool recieve();
    bool send(std::string const&);
    void skip_send();

    bool send_leaved();
    bool send_bad_vote();
    bool send_greetings(unsigned int users_online);

    int get_message_type() const;

    void lock();
    void unlock();

    explicit Client(SOCKET s);
    ~Client();
};

