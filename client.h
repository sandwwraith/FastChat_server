#pragma once
#include <WinSock2.h>
#include <string>
//Maximum buffer size for I/O Operations
#define MAX_BUFFER_SIZE 256

//Operation codes for clients
#define OP_SEND 1
//Server sends data to client
#define OP_RECV 2
//Server recievs data

//Client statuses
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

class Client
{
private:
    //Field, necessary for overlapped (async) operations
    OVERLAPPED *overlapped;

    //Buffer with data
    WSABUF *wsabuf;

    //Socket of client
    SOCKET socket;
    
public:
    int op_code;
    int id;
    int client_status;

    Client* companion = nullptr;

    char* get_buffer_data();
    int get_buffer_size() const;
    WSABUF* get_wsabuff_ptr();
    OVERLAPPED* get_overlapped_ptr();
    void reset_buffer();
    SOCKET get_socket();

    bool send_companion_buffer();
    //Remember, this will reset your buffer
    bool recieve();
    bool send(std::string const&);

    int get_message_type() const;

    explicit Client(SOCKET s);
    ~Client();
};

