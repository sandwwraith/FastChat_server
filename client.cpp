#include "client.h"
#include <iostream>
#include <thread>


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
    std::cout << id << " receiving...(#" << std::this_thread::get_id() << std::endl;
    DWORD dwBytes, dwFlags = 0;
    reset_buffer();
    this->op_code = OP_RECV;
    int snd = WSARecv(this->get_socket(), this->get_wsabuff_ptr(), 1, &dwBytes, &dwFlags, this->get_overlapped_ptr(), nullptr);
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

bool Client::send(std::string const & message)
{
    std::cout << id << " sending...(#" << std::this_thread::get_id() << std::endl;
    this->reset_buffer();
    CopyMemory(wsabuf->buf, message.c_str(), message.length());
    wsabuf->len = message.length();

    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    op_code = OP_SEND;
    auto snd = WSASend(this->socket, wsabuf, 1, &dwBytes, dwFlags, overlapped, nullptr);
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

int Client::get_message_type() const
{
    return this->wsabuf->buf[1];
}

SOCKET Client::get_socket()
{
    return socket;
}

bool Client::send_companion_buffer()
{
    std::cout << id << " sending...(#" << std::this_thread::get_id() << std::endl;
    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    op_code = OP_SEND;
    auto snd = WSASend(this->socket, companion->wsabuf, 1, &dwBytes, dwFlags, overlapped, nullptr);
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

Client::Client(SOCKET s) : socket(s)
{
    client_status = STATE_NEW;
    overlapped = new OVERLAPPED;
    wsabuf = new WSABUF;

    ZeroMemory(overlapped, sizeof(OVERLAPPED));
    ZeroMemory(wsabuf, sizeof(WSABUF));
    wsabuf->buf = static_cast<char*>(malloc(MAX_BUFFER_SIZE*sizeof(char)));
    reset_buffer();
}

Client::~Client()
{
    std::cout << id << " destroyed\n";
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
