#pragma once
#include <WinSock2.h>
#include <assert.h>

//Maximum buffer size for I/O Operations
// TODO: make this const
#define MAX_BUFFER_SIZE 256

struct CLIENT_BUFFER
{
    WSABUF recv_buf;
    WSABUF send_buf;
    
    CLIENT_BUFFER(const CLIENT_BUFFER& other) = delete;
    CLIENT_BUFFER& operator=(const CLIENT_BUFFER& other) = delete;
    
    CLIENT_BUFFER()
    {
        recv_buf.buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        if (recv_buf.buf == nullptr) throw std::runtime_error("Memory error at buffer init");
        recv_buf.len = MAX_BUFFER_SIZE;

        // TODO: currently socket_user holds one CLIENT_BUFFER, and CLIENT_BUFFER holds two WSABUF
        // in constructor CLIENT_BUFFER initialize both WSABUFs
        // in destructor CLIENT_BUFFER release both WSABUFs
        // it has two reset functions for each WSABUF
        // probably instead of CLIENT_BUFFER with two WSABUF we should have CLIENT_BUFFER with one WSABUF
        // and hold two CLIENT_BUFFERs in socket_user?
        send_buf.buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        if (send_buf.buf == nullptr) {
            free(recv_buf.buf);
            throw std::runtime_error("Memory error at buffer init");
        }
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

    void fill_send_buf(std::string const& message) noexcept
    {
        //this->reset_snd_buf();
        ULONG len = min(message.length(), MAX_BUFFER_SIZE);
        CopyMemory(send_buf.buf, message.c_str(), len);
        send_buf.len = len;
    }
};