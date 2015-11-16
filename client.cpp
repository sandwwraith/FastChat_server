#include "client.h"


char* Client::get_buffer_data() const
{
    return wsabuf->buf;
}

int Client::get_buffer_size() const
{
    return wsabuf->len;
}

WSABUF* Client::get_wsabuff_ptr() const
{
    return wsabuf;
}

OVERLAPPED* Client::get_overlapped_ptr() const
{
    return overlapped;
}

void Client::reset_buffer()
{
    wsabuf->len = MAX_BUFFER_SIZE;
    ZeroMemory(wsabuf->buf, wsabuf->len);
}

SOCKET Client::get_socket()
{
    return socket;
}

Client::Client(SOCKET s) : socket(s)
{
    overlapped = new OVERLAPPED;
    wsabuf = new WSABUF;

    ZeroMemory(overlapped, sizeof(OVERLAPPED));
    //overlapped->hEvent = NULL;// i dont believe in magic
    ZeroMemory(wsabuf, sizeof(WSABUF));
    wsabuf->buf = (char*)malloc(MAX_BUFFER_SIZE*sizeof(char));
    reset_buffer();
}

Client::~Client()
{
    //Wait for the pending operations to complete
    while (!HasOverlappedIoCompleted(overlapped))
    {
        Sleep(1);
    }

    closesocket(socket);

    delete overlapped;
    reset_buffer();
    free(wsabuf->buf);
    delete wsabuf;
}
