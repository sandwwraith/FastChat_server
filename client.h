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
    bool new_client;
    std::string last_message;

    char* get_buffer_data();
    int get_buffer_size() const;
    WSABUF* get_wsabuff_ptr();
    OVERLAPPED* get_overlapped_ptr();
    void reset_buffer();
    SOCKET get_socket();

   
    //Remember, this will reset your buffer
    bool recieve();
    bool send(std::string);

    //Call it when you've received smth. See ATTACH_RESULT for details.
    ATTACH_RESULT attach_bytes_to_message();

    explicit Client(SOCKET s);
    ~Client();
};

