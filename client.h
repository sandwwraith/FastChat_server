#pragma once
#include <WinSock2.h>
#include <string>
//Maximum buffer size for I/O Operations
#define MAX_BUFFER_SIZE 256

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
    std::string client_message;

    char* get_buffer_data() const;
    int get_buffer_size() const;
    WSABUF* get_wsabuff_ptr() const;
    OVERLAPPED* get_overlapped_ptr() const;
    void reset_buffer();
    SOCKET get_socket();

    explicit Client(SOCKET s);
    ~Client();
};

