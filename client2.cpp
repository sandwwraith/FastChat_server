#include "stdafx.h"

#include "server.h"
#include "client2.h"

void socket_user::send(std::string const& message)
{
    buf.fill_send_buf(message);
    DWORD dwBytes = 0;
    DWORD dwFlags = 0;
    auto res = WSASend(this->sock, &buf.send_buf, 1, &dwBytes, dwFlags, snd, nullptr);
    if (res == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) throw std::runtime_error("Send error");
}

void socket_user::recv()
{
    DWORD dwBytes = 0, dwFlags = 0;
    buf.reset_recv_buf();
    int snd = WSARecv(this->sock, &buf.recv_buf, 1, &dwBytes, &dwFlags, rcv, nullptr);
    if (snd == SOCKET_ERROR && WSA_IO_PENDING != WSAGetLastError()) throw std::runtime_error("Recieve error");
}

std::string socket_user::read(unsigned bytes_count)
{
    return std::string{ buf.recv_buf.buf,bytes_count };
}

socket_user::socket_user(SOCKET s) : sock(s)
{
    snd = new OVERLAPPED_EX(operation_code::SEND);
    rcv = new OVERLAPPED_EX(operation_code::RECV);
}

socket_user::~socket_user()
{
    if (HasOverlappedIoCompleted(snd)) delete snd;
    else snd->op_code = operation_code::DELETED;

    if (HasOverlappedIoCompleted(rcv)) delete rcv;
    else rcv->op_code = operation_code::DELETED;

    closesocket(sock);
}

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
        if (res == DISCONNECT) host->drop_client(this);
    } 
    catch (std::exception& e)
    {
        std::cout <<"Client #"<< ptr->id <<" error: " << e.what() << std::endl;
        host->drop_client(this);
    }

}

client_res Client::on_recv_finished(unsigned bytesTransfered)
{
    std::string msg{ handle.read(bytesTransfered) };
    std::cout << "Client " << id << " send " << msg << std::endl;
    if (msg[0] == 3) //disconnect 
        return DISCONNECT;
    handle.send(msg);
    handle.recv();
    return OK;
}

client_res Client::on_send_finished(unsigned bytesTransfered)
{
    //no action
    return OK;
}

bool Client::send_greetings(unsigned)
{
    handle.send("Greetings, traveller!\n");
    handle.recv();
    return true;
}