#include "client.h"


char* Client::get_buffer_data()
{
    return wsabuf->buf;
}

int Client::get_buffer_size() const
{
    return wsabuf->len;
}

WSABUF* Client::get_wsabuff_ptr()
{
    return wsabuf;
}

OVERLAPPED* Client::get_overlapped_ptr()
{
    return overlapped;
}

void Client::reset_buffer()
{
    wsabuf->len = MAX_BUFFER_SIZE;
    ZeroMemory(wsabuf->buf, wsabuf->len);
}

bool Client::recieve()
{
    DWORD dwBytes, dwFlags = 0;
    reset_buffer();
    int snd = WSARecv(this->get_socket(), this->get_wsabuff_ptr(), 1, &dwBytes, &dwFlags, this->get_overlapped_ptr(), nullptr);
    this->op_code = OP_RECV;
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

bool Client::send(std::string message)
{
    this->reset_buffer();
    CopyMemory(wsabuf->buf, message.c_str(), message.length());
    wsabuf->len = message.length();

    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    auto snd = WSASend(this->socket, wsabuf, 1, &dwBytes, dwFlags, overlapped, nullptr);
    op_code = OP_SEND;
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

ATTACH_RESULT Client::attach_bytes_to_message()
{
    std::string data = this->get_buffer_data();
    if (data.back() == 8 /*backspace*/) 
    {
        last_message.pop_back();
        return MESSAGE_INCOMPLETE;
    }
    if (data.back() == 3 /*disconnect */ )
    {
        return CLIENT_DISCONNECT;
    }
    last_message.append(data);
    return data[data.size() - 1] < 32 ? MESSAGE_COMPLETE : MESSAGE_INCOMPLETE;
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
