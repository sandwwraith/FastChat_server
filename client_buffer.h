#pragma once
#include <WinSock2.h>
#include <assert.h>

//Maximum buffer size for I/O Operations
#define MAX_BUFFER_SIZE 256
#include <utility>

struct CLIENT_BUFFER
{
    WSABUF recv_buf;
    WSABUF send_buf;
    
    CLIENT_BUFFER(const CLIENT_BUFFER& other) = delete;
    CLIENT_BUFFER& operator=(const CLIENT_BUFFER& other) = delete;
    
    CLIENT_BUFFER()
    {
        recv_buf.buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        recv_buf.len = MAX_BUFFER_SIZE;

        send_buf.buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        send_buf.len = MAX_BUFFER_SIZE;
    }

    ~CLIENT_BUFFER()
    {
        free(recv_buf.buf);
        free(send_buf.buf);
    }

    void reset_snd_buf()
    {
        ZeroMemory(send_buf.buf, MAX_BUFFER_SIZE);
        send_buf.len = MAX_BUFFER_SIZE;
    }

    void reset_recv_buf()
    {
        ZeroMemory(recv_buf.buf, MAX_BUFFER_SIZE);
        recv_buf.len = MAX_BUFFER_SIZE;
    }

    void fill_send_buf(std::string const& message)
    {
        this->reset_snd_buf();
        assert(message.length() < MAX_BUFFER_SIZE);
        CopyMemory(send_buf.buf, message.c_str(), message.length());
        send_buf.len = message.length();
    }
};