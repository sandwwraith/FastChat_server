#include "server.h"

int main()
{
    std::cout << "Launching...\n";
    server serv;
    serv.init();
    serv.main_cycle();
    return 0;
}