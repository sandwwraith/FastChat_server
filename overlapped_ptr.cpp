#include "stdafx.h"
#include "overlapped_ptr.h"

void overlapped_deleter::operator()(OVERLAPPED_EX* ptr) const
{
    if (HasOverlappedIoCompleted(ptr))
        delete ptr;
    else
        ptr->op_code = operation_code::DELETED;
}

overlapped_ptr make_overlapped(operation_code op_code)
{
    return overlapped_ptr{new OVERLAPPED_EX{op_code}};
}
