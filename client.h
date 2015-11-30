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

//Bytes operation results types
#define MESSAGE_COMPLETE 0x1
#define MESSAGE_INCOMPLETE 0x2
#define CLIENT_DISCONNECT 0x4
typedef int ATTACH_RESULT;

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
    std::string last_message;

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

    //Call it when you've received smth. See ATTACH_RESULT for details.
    ATTACH_RESULT attach_bytes_to_message();

    explicit Client(SOCKET s);
    ~Client();
};

