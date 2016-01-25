#pragma once
#include <WinSock2.h>
#include <assert.h>

//Maximum buffer size for I/O Operations
static const int MAX_BUFFER_SIZE = 1024;

struct CLIENT_BUFFER
{
    WSABUF buffer;
    
    CLIENT_BUFFER(const CLIENT_BUFFER& other) = delete;
    CLIENT_BUFFER& operator=(const CLIENT_BUFFER& other) = delete;
    
    CLIENT_BUFFER()
    {
        buffer.buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
        if (buffer.buf == nullptr) throw std::runtime_error("Memory error at buffer init");
        buffer.len = MAX_BUFFER_SIZE;
    }

    ~CLIENT_BUFFER()
    {
        free(buffer.buf);
    }

    void reset()
    {
        ZeroMemory(buffer.buf, MAX_BUFFER_SIZE);
        buffer.len = MAX_BUFFER_SIZE;
    }

    void fill_buf(std::string const& message) noexcept
    {
        ULONG len = min(message.length(), MAX_BUFFER_SIZE);
        CopyMemory(buffer.buf, message.c_str(), len);
        buffer.len = len;
    }
};