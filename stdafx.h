#ifndef STDAFX_H_SERVER
#define STDAFX_H_SERVER

#include <stdio.h>
#include <stdlib.h>
#include <winsock2.h>
#include <mswsock.h>
#include <string>
#include <iostream>
#include <list>

#include <thread>
#include <mutex>
#include <memory>
#include <array>

inline uint64_t current_time() noexcept 
{
    return std::chrono::system_clock::now().time_since_epoch().count() 
        * std::chrono::system_clock::period::num / std::chrono::system_clock::period::den;
}
#endif