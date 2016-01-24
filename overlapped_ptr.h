#pragma once
#include "client2.h"
#include <memory>

struct overlapped_deleter
{
    void operator()(OVERLAPPED_EX* ptr) const;
};

using overlapped_ptr = std::unique_ptr<OVERLAPPED_EX, overlapped_deleter>;

overlapped_ptr make_overlapped(operation_code op_code);