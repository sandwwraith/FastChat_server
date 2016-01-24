#pragma once

#include <WinSock2.h>

//Operation codes for clients
enum class operation_code : unsigned
{
    //Server sends data to client
    SEND = 1,
    //Server recievs data
    RECV = 2,
    //Special codes
    KEEP_ALIVE = 10,
    ACCEPT = 11,
    DELETED = 12,
};

struct OVERLAPPED_EX : OVERLAPPED
{
    operation_code op_code;

    explicit OVERLAPPED_EX(operation_code op_code) : OVERLAPPED{}, op_code(op_code){}
};
