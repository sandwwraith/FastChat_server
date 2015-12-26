#include "stdafx.h"
#include "client.h"

std::weak_ptr<Client>& Client::get_companion()
{
    return companion;
}

void Client::set_companion(std::weak_ptr<Client> const& c)
{
    companion = c;
}

bool Client::has_companion() const
{
    return !companion.expired();
}

char* Client::get_recv_buffer_data()
{
    return buffer.recv_buf.buf;
}

bool Client::recieve() noexcept
{
    std::cout << id << " receiving...(#" << std::this_thread::get_id() << std::endl;
    DWORD dwBytes = 0, dwFlags = 0;
    buffer.reset_recv_buf();
    int snd = WSARecv(this->socket, &buffer.recv_buf, 1, &dwBytes, &dwFlags, overlapped_recv, nullptr);
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

bool Client::send_greetings(unsigned int users_online)
{
    static_assert(sizeof(int) == 4,"Sizeof(int) doesn't equals 4");

    std::string buf;
    buf.push_back(42);
    buf.push_back(users_online >> 24 & 0xFF);
    buf.push_back(users_online >> 16 & 0xFF);
    buf.push_back(users_online >> 8 & 0xFF);
    buf.push_back(users_online & 0xFF);
    return this->send(buf);
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

bool Client::send(std::string const & message) noexcept
{
    std::cout << id << " sending...(#" << std::this_thread::get_id() << std::endl;
     
    buffer.fill_send_buf(message);
    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    auto snd = WSASend(this->socket, &buffer.send_buf, 1, &dwBytes, dwFlags, overlapped_send, nullptr);
    return !(snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError());
}

bool Client::safe_comp_send(std::string const& msg) noexcept
{
    auto comp = companion.lock();
    if (comp.operator bool())
    {
        comp->send(msg);
        return true;
    }
    return false;
}

int Client::get_snd_message_type() const
{
    return this->buffer.send_buf.buf[1];
}

int Client::get_recv_message_type() const
{
    return this->buffer.recv_buf.buf[1];
}

SOCKET Client::get_socket()
{
    return socket;
}

Client::Client(SOCKET s) : socket(s)
{
    client_status = STATE_NEW;
    overlapped_recv = new OVERLAPPED_EX{operation_code::RECV};
    overlapped_send = new OVERLAPPED_EX{operation_code::SEND};
    overlapped_special = new OVERLAPPED_EX{operation_code::KEEP_ALIVE};
}

Client::~Client()
{
    std::cout << id << " destroyed\n";

    if (HasOverlappedIoCompleted(overlapped_recv)) delete overlapped_recv;

    closesocket(socket);

    if (HasOverlappedIoCompleted(overlapped_send)) delete overlapped_send;

    overlapped_special->op_code = operation_code::DELETED;
}
