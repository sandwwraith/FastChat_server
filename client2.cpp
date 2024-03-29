#include "stdafx.h"

#include "server.h"
#include "client2.h"

void socket_user::send(std::string const& message)
{
    snd_buf.fill_buf(message);
    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    auto res = WSASend(this->sock, &snd_buf.buffer, 1, &dwBytes, dwFlags, snd.get(), nullptr);
    if (res == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) throw std::runtime_error("Send error");
}

void socket_user::recv()
{
    DWORD dwBytes = 0, dwFlags = 0;
    rcv_buf.reset();
    int snd = WSARecv(this->sock, &rcv_buf.buffer, 1, &dwBytes, &dwFlags, rcv.get(), nullptr);
    if (snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) throw std::runtime_error("Recieve error");
}

std::string socket_user::read(unsigned bytes_count)
{
    return std::string{ rcv_buf.buffer.buf,bytes_count };
}

socket_user::socket_user(SOCKET s)
    : sock(s)
    , snd(new OVERLAPPED_EX(operation_code::SEND))
    , rcv(new OVERLAPPED_EX(operation_code::RECV))
{ }

socket_user::~socket_user()
{ }

void client_context::on_overlapped_io_finished(unsigned bytesTransfered, OVERLAPPED_EX* overlapped) noexcept
{
    try {
        client_res res = OK;
        switch (overlapped->op_code)
        {
        case operation_code::RECV:
            res = ptr->on_recv_finished(bytesTransfered);
            break;
        case operation_code::SEND:
            res = ptr->on_send_finished(bytesTransfered);
            break;
        default:
            std::cout << "Unknown opcode\n";
        }
        if (res == QUEUE_OP) host->handle_queue_request(ptr, bytesTransfered);
        if (res == DISCONNECT)
        {
            std::cout << ptr->id << " send disconnect"<<std::endl;
            host->drop_client(this);
        }
    } 
    catch (std::exception& e)
    {
        std::cout <<"Client #"<< ptr->id <<" error: " << e.what() << std::endl;
        host->drop_client(this);
    }

}

client_context::client_context(server* serv, Client* cl) 
    : host(serv)
    , ptr(cl)
    , over(ptr ? new OVERLAPPED_EX{operation_code::KEEP_ALIVE} : nullptr)
    //client_context with client==nullptr is used as dummy context for ACCEPT event
    //Therefore, we can't create new KEEP_ALIVE for such dummy client, 'cause it will
    //never get to function queue->never get to IOCP -> never be deleted
{ }

client_context::~client_context()
{ }

bool client_context::isAlive() const noexcept
{
    return (current_time() - lastActivity) < MAX_IDLENESS_TIME;
}

void client_context::updateTimer() noexcept
{
    lastActivity = current_time();
}

std::function<void()> client_context::get_upd_f() noexcept
{
    //Save parametres as members of anonymous class, because THIS can be deleted at moment of run operator()
    OVERLAPPED_EX* over_ptr = this->over.get();
    server* serv = this->host;
    return [=]()
        {
            serv->IOCP.post(this, over_ptr, 1);
        };
}

client_res Client::on_recv_finished(unsigned bytesTransfered)
{
    std::string msg{ handle.read(bytesTransfered) };
    std::cout << "Client " << id << " send " << msg << std::endl;
    if (get_recv_message_type() == message_type::DISCONNECT)
    {
        return DISCONNECT;
    }
    switch(status)
    {
    case INIT:
        if (get_recv_message_type() != message_type::QUEUE || q_msg.size() > 0)
        {
            handle.recv();
            break;
        }
        this->q_msg = msg;
        handle.recv();
        return QUEUE_OP;
    case MESSAGING:
        
        if (this->get_recv_message_type() == message_type::TIMEOUT)
            status = VOTING;
        if (this->get_recv_message_type() == message_type::LEAVE)
            status = INIT;
        if (!this->safe_comp_send(msg))
        {
            //Handling UNEXPECTEDLY leave
            this->send_leaved();
        }

        handle.recv();
        break;
    case VOTING:
        if (get_recv_message_type()!= message_type::VOTING)
        {
            handle.recv();
            break;
        }
        status = INIT;
        if (!this->safe_comp_send(msg)) this->send_bad_vote();
        this->companion = std::weak_ptr<Client>();
        handle.recv();
    default:
        break;
    }
    return OK;
}

client_res Client::on_send_finished(unsigned bytesTransfered)
{
    switch(status)
    {
    case NEW:
        status = INIT;
        handle.recv();
        break;
    case INIT:
        if (get_snd_message_type() == message_type::QUEUE)
        {
            auto p = companion.lock();
            if (p)
            {
                p->q_msg.resize(0);
                status = MESSAGING;
            }
        }
        break;
    case MESSAGING:
        if (this->get_snd_message_type() == message_type::TIMEOUT)
            status = VOTING;
        if (this->get_snd_message_type() == message_type::LEAVE)
            status = INIT;
    default:
        break;
    }
    return OK;
}

void Client::on_pair_found(std::shared_ptr<Client>const& pair)
{
    this->companion = std::weak_ptr<Client>{ pair };
    handle.send(pair->q_msg);
}

bool Client::safe_comp_send(std::string const& msg)
{
    auto comp = companion.lock();
    if (comp)
    {
        comp->handle.send(msg);
        return true;
    }
    return false;
}

bool Client::send_bad_vote()
{
    std::string s = { 42, static_cast<char>(message_type::VOTING), 0 };
    handle.send(s);
    return true;
}

bool Client::send_leaved()
{
    std::string s = { 42, static_cast<char>(message_type::LEAVE) };
    handle.send(s);
    return true;
}

message_type Client::get_snd_message_type() const
{
    return static_cast<message_type>(this->handle.snd_buf.buffer.buf[1]);
}

message_type Client::get_recv_message_type() const
{
    return static_cast<message_type>(this->handle.rcv_buf.buffer.buf[1]);
}

void Client::set_theme(char theme)
{
    this->q_msg[2] = theme;
}

bool Client::send_greetings(unsigned users_online)
{
    static_assert(sizeof(int) == 4, "Sizeof(int) doesn't equals 4");

    std::string buf;
    buf.push_back(42);
    buf.push_back(users_online >> 24 & 0xFF);
    buf.push_back(users_online >> 16 & 0xFF);
    buf.push_back(users_online >> 8 & 0xFF);
    buf.push_back(users_online & 0xFF);
    handle.send(buf);
    return true;
}