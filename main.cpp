#include "server.h"
#include <algorithm>
int main(int argc, char* argv[])
{
    //Validataing args
    bool global = false;
    if (argc > 1)
    {
        std::string data(argv[1]);
        std::transform(data.begin(), data.end(), data.begin(), ::tolower);
        if (data == "global") global = true;
    }

    std::cout << "Launching " << (global ? "global..." : "local...") << std::endl;
    server serv;
    serv.set_global_addr(global);
    serv.init();
    serv.main_cycle();
    return 0;
}