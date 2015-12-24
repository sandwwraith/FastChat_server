#pragma once
#include <WinSock2.h>

//Maximum buffer size for I/O Operations
#define MAX_BUFFER_SIZE 256

struct CLIENT_BUFFER
{
    WSABUF *recv_buf;
    WSABUF *send_buf;

    CLIENT_BUFFER()
    {
        recv_buf = new WSABUF;
        ZeroMemory(recv_buf, sizeof(WSABUF));
        recv_buf->buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        recv_buf->len = MAX_BUFFER_SIZE;

        send_buf = new WSABUF;
        ZeroMemory(send_buf, sizeof(WSABUF));
        send_buf->buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        send_buf->len = MAX_BUFFER_SIZE;
    }

    ~CLIENT_BUFFER()
    {
        free(recv_buf->buf);
        free(send_buf->buf);

        delete send_buf;
        delete recv_buf;
    }

    void reset_snd_buf()
    {
        ZeroMemory(send_buf->buf, MAX_BUFFER_SIZE);
        send_buf->len = MAX_BUFFER_SIZE;
    }

    void reset_recv_buf()
    {
        ZeroMemory(recv_buf->buf, MAX_BUFFER_SIZE);
        recv_buf->len = MAX_BUFFER_SIZE;
    }

    void fill_send_buf(std::string const& message)
    {
        this->reset_snd_buf();
        CopyMemory(send_buf->buf, message.c_str(), message.length());
        send_buf->len = message.length();
    }
};