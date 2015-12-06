#include "client.h"
#include <iostream>
#include <thread>


Client* Client::get_companion() const
{
    return companion;
}

void Client::delete_companion()
{
    this->lock();
    companion = (nullptr);
    this->unlock();
}

bool Client::own_companion()
{
    this->lock();
    if (this->has_companion())
    {
        return true;
    }
    this->unlock();
    return false;
}

bool Client::has_companion()
{
    return companion!=nullptr;
}

void Client::set_companion(Client* cmp)
{
    companion = cmp;
}

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

void Client::skip_send()
{
    this->reset_buffer();
    this->op_code = OP_RECV;
}

bool Client::send_bad_vote()
{
    std::string s = {42, MST_VOTING, 0};
    return send(s);
}

bool Client::send_leaved()
{
    std::string s = { 42, MST_LEAVE };
    return send(s);
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

void Client::lock()
{
    EnterCriticalSection(&cl_sec);
}

void Client::unlock()
{
    LeaveCriticalSection(&cl_sec);
}

int Client::get_message_type() const
{
    return this->wsabuf->buf[1];
}

SOCKET Client::get_socket()
{
    return socket;
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

    InitializeCriticalSection(&cl_sec);
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
    DeleteCriticalSection(&cl_sec);
}
